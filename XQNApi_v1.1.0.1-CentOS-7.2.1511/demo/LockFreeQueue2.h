#ifndef CIRCLE_QUEUE_H
#define CIRCLE_QUEUE_H

#include <assert.h>
#include <string.h>

#ifdef WIN32
#include <windows.h>
#include <winbase.h>
#include <apr_atomic.h>
#else
#include <sys/types.h>
#include <unistd.h>
#include <sched.h>
#endif//WIN32

typedef unsigned int uint32_t;
typedef int			 int32_t;

// the default size of the free-lock queue
#ifndef ARRAY_LOCK_FREE_Q_DEFAULT_SIZE
#define ARRAY_LOCK_FREE_Q_DEFAULT_SIZE	4096 * 128
#endif

#define SINGLE_THREAD_TRUE	1		// single thread operated flag
#define SINGLE_THREAD_FALSE	0		// not single thread operated flag
#define MULTI_THREAD_TRUE	SINGLE_THREAD_FALSE
#define MULTI_THREAD_FALSE	SINGLE_THREAD_TRUE


/// define the atomic operations by different platforms
#ifdef WIN32
/// declare the automic operations on win32 platform
/// @brief atomically adds count to the variable pointed by ptr
/// @return the value that had previously been in memory
#define AtomicAdd(ptr, count) (InterlockedExchangeAdd((LONG*)(ptr), (count)))

/// @brief atomically substracts count from the variable pointed by ptr
/// @return the value that had previously been in memory
#define AtomicDec(ptr, count) (InterlockedExchangeAdd((LONG*)(ptr), -(count)))

/// @brief atomically exchange ptr's value with val
/// @return the value that had previously been in memory
#define AtomicXchg(ptr, val) (InterlockedExchange((LONG*)(ptr), val))

/// @brief Compare And Swap
/// If the current value of *ptr is equal to compVal, then write newVal into *ptr
/// @return true if the comparison is successful and newVal was written
#if 0
#if _MSC_VER > 1200
#define CAS(ptr, compVal, newVal) ((compVal) == InterlockedCompareExchange((LONG*)(ptr), (newVal), (compVal)))
#else
#define CAS(ptr, compVal, newVal) (PVOID(compVal) == InterlockedCompareExchange((PVOID*)(ptr), PVOID(newVal), PVOID(compVal)))
#endif // _MSC_VER > 1000
#else
#define CAS(ptr, compVal, newVal) ((compVal) == apr_atomic_cas32((ptr), (newVal), (compVal)))
#endif//0-apr

/// @brief Compare And Swap
/// If the current value of *ptr is oldVal, then write newVal into *ptr
/// @return the contents of *ptr before the operation
#define CASVal(ptr, compVal, newVal) (InterlockedCompareExchange((ptr), (newVal), (compVal)))

#else
#ifdef __GNUC__
// Atomic functions in GCC are present from version 4.1.0 on
// http://gcc.gnu.org/onlinedocs/gcc-4.1.0/gcc/Atomic-Builtins.html

// Test for GCC >= 4.1.0
#if (__GNUC__ < 4) || \
          ((__GNUC__ == 4) && ((__GNUC_MINOR__ < 1) || \
          ((__GNUC_MINOR__     == 1) && \
          (__GNUC_PATCHLEVEL__ < 0))) )

#error Atomic built-in functions are only available in GCC in versions >= 4.1.0
#endif // end of check for GCC 4.1.0

/// @brief atomically adds a_count to the variable pointed by a_ptr
/// @return the value that had previously been in memory
#define AtomicAdd(ptr, count) __sync_fetch_and_add((uint32_t*)(ptr), (count))

/// @brief atomically substracts a_count from the variable pointed by a_ptr
/// @return the value that had previously been in memory
#define AtomicDec(ptr, count) __sync_fetch_and_sub((ptr), (count))

/// @brief atomically exchange ptr's value with val
/// @return the value that had previously been in memory
#define AtomicXchg(ptr, val) __sync_lock_test_and_set((uint32_t*)(ptr), val)

/// @brief Compare And Swap
/// If the current value of *ptr is equal to compVal, then write newVal into *ptr
/// @return true if the comparison is successful and newVal was written
#define CAS(ptr, compVal, newVal) __sync_bool_compare_and_swap((ptr), (compVal), (newVal))

/// @brief Compare And Swap
/// If the current value of *a_ptr is a_oldVal, then write a_newVal into *a_ptr
/// @return the contents of *a_ptr before the operation
#define CASVal(ptr, compVal, newVal) __sync_val_compare_and_swap((ptr), (compVal), (newVal))

#else
#error Atomic functions such as CAS or AtomicAdd are not defined for your compiler. Please add them in atomic_ops.h
#endif//__GNUC__

#endif //WIN32

namespace LOCK_FREE
{

/// @brief thread yield to give up CPU time slice
inline void thread_yield()
{
#ifdef WIN32
	Sleep(0);
#else
	sched_yield();
#endif//WIN32
}


/*
* Lock-Free queue based on a circular array
* Q_SIZE should be power of 2
* SingleIn: single producer thread operated "push", default is true
* SingleOut:single consumer thread operated "pop",  default is false
*/
template<typename Ty,
		 uint32_t SingleIn	= SINGLE_THREAD_FALSE,
		 uint32_t SingleOut	= SINGLE_THREAD_FALSE>
class ArrayLockFreeQueue
{
	typedef Ty ElemType;
public:
	ArrayLockFreeQueue(uint32_t maxQueSize = ARRAY_LOCK_FREE_Q_DEFAULT_SIZE)
	{
		maxSize_	= maxQueSize;
		theQue_		= new ElemType[maxSize_];
		memset(theQue_, 0, sizeof(ElemType) * maxSize_);
		readIndex_	= writeIndex_ = 0;
		maxReadIndex_ = 0;
	}
	~ArrayLockFreeQueue()
	{
		readIndex_	= writeIndex_ = 0;
		maxReadIndex_ = 0;
		delete []theQue_;
	}

	/// @brief push an element at the tail of the queue
	/// @param the element to insert in the queue
	/// @returns true if the element was inserted in the queue. false if the queue was full
	bool push(const ElemType& theData)
	{
		if (SingleIn)
		{
			// if queue if full
			if (countToIndex(writeIndex_ + 1) == readIndex_)
				return false;
			
			// since single producer, no atomic operation to get write index
			// save the data at the index which is reserved for us
			theQue_[writeIndex_] = theData;
			
			// increase the write index
			writeIndex_ = countToIndex(writeIndex_ + 1);
			
			// update the maximum read index after saving the data
			// then, consumer thread can access the data of this index
			maxReadIndex_ = countToIndex(maxReadIndex_ + 1);
			return true;
		}
		else
		{
			uint32_t curWriteIndex;
			uint32_t curReadIndex;
			
			// find the index to be inserted to
			do
			{
				curWriteIndex	= writeIndex_;
				curReadIndex	= readIndex_;
				// if queue is full
				if (countToIndex(curWriteIndex + 1) == curReadIndex)
					return false ;
			} while ( !CAS(&writeIndex_, curWriteIndex, countToIndex(curWriteIndex + 1)) );
			
			
			// save the data since the current write index is reserved for us
			theQue_[curWriteIndex] = theData;
			
			// update the maximum read index after saving the data.
			// It wouldn't fail if there is only one producer thread intserting data into the queue.
			// It will failed once more than 1 producer threads because
			// the maxReadIndex_ should be done automic as the previous CAS
			while ( !CAS(&maxReadIndex_, curWriteIndex, countToIndex(curWriteIndex + 1)) )
			{
				thread_yield();
			}
			return true;
		}//SingleIn
	}
	bool pop(ElemType& theData)
	{
		if (SingleOut)
		{
			// if queue is empty
			if (readIndex_ == maxReadIndex_)
				return false;			
			// since single consumer, just get read index to retrieve data from the queue
			theData		= theQue_[readIndex_];
			readIndex_	= countToIndex(readIndex_ + 1);
			return true;
		}
		else
		{
			uint32_t curMaxReadIndex;
			uint32_t curReadIndex;
			
			// find the valid index to be read
			do
			{
				// to ensure thread-safety when there is more than 1 producer threads
				// a second index is defined: maxReadIndex_
				curReadIndex     = readIndex_;
				curMaxReadIndex  = maxReadIndex_;			
				// if the queue is empty or
				// a producer thread has occupied space in the queue,
				// but is waiting to commit the data into it
				if (curReadIndex == curMaxReadIndex)
					return false ;
				
				// retrieve the data from the queue
				theData = theQue_[curReadIndex];
				
				// we automic increase the readIndex_ using CAS operation
				if ( CAS(&readIndex_, curReadIndex, countToIndex(curReadIndex + 1)) )
					return true ;
				
				// here, if failed retrieving the element off the queue
				// someone else is reading the element at curReadIndex before we perform CAS operation
				thread_yield();
			} while (true); // keep looping to try again
			
			// to avoid compile warning
			return false;
		}//SingleOut
	}
	bool isEmpty() const { return (readIndex_ == maxReadIndex_) ? true : false; }

	/// @return elements count which may be bogus
	uint32_t size() const
	{
		uint32_t curWriteIndex	= writeIndex_;
		uint32_t curReadIndex	= readIndex_;
		
		// curWriteIndex may be at the front of curReadIndex
		if (curWriteIndex >= curReadIndex)
		{
			return curWriteIndex - curReadIndex;
		}
		else
		{
			return (maxSize_ + curWriteIndex - curReadIndex);
		}
		return 0;
	}
	
protected:
	/// @brief calculate the index in the circular array that corresponds
	/// to a particular "count" value
	inline uint32_t countToIndex(uint32_t passIndex){
		//return (passIndex & (maxSize_ - 1));
		if (passIndex < maxSize_)
			return passIndex;
		return (passIndex - maxSize_);
	}
private:
	ElemType*	theQue_;				// array to keep the elements
	uint32_t	maxSize_;				// the max queue size
	uint32_t	readIndex_;				// where the next element where be extracted from
	uint32_t	writeIndex_;			// where a new element will be inserted to
	
	/// @brief maximum read index for multiple producer queues
	/// If it's not the same as m_writeIndex it means
	/// there are writes pending to be "committed" to the queue, that means,
	/// the place for the data was reserved (the index in the array) but
	/// data is still not in the queue, so the thread trying to read will have
	/// to wait for those other threads to save the data into the queue
	///
	/// note this index is only used for MultipleProducerThread queues
	uint32_t	maxReadIndex_;
};//ArrayLockFreeQueue


}//LOCK_FREE

#endif//CIRCLE_QUEUE_H
