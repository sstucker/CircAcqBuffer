#pragma once
#include <cstdint>
#include <atomic>
#include <mutex>

// Push-only ring buffer inspired by buffer interface of National Instruments IMAQ software. Elements
// pushed to the ring are given a count corresponding to the number of times push() has been called
// since the buffer was initialized. A push() constitutes a copy into buffer-managed memory. The n-th element
// can be locked out of the ring for processing, copy or display and then subsequently released. If the n-th
// element isn't available yet or has been overwritten, the buffer where the n-th element would have been
// is returned instead along with the count of the element you have actually locked out. Somewhat thread-safe but
// only designed for single-producer single-consumer use.
//
// sstucker 2021
//

inline int mod2(int a, int b)
{
	int r = a % b;
	return r < 0 ? r + b : r;
}

template <typename T>
struct CircAcqElement
{
	T* arr;  // the buffer
	unsigned int index;  // position of data in ring 
	long count;  // the count of the data currently in the buffer
};


template <class T>
class CircAcqBuffer
{
protected:

	CircAcqElement<T>** ring;
	CircAcqElement<T>* locked_out_buffer;
	unsigned int ring_size;
	unsigned int element_size;
	int head;
	long count;  // cumulative count
	std::mutex* locks;  // Locks on each ring pointer
	std::atomic_int locked;  // index of currently locked buffer
	
	inline void _lock_out(unsigned int n)
	{
		locked.store(n);  // Update locked out value

		// Pointer swap
		CircAcqElement<T>* tmp = locked_out_buffer;
		locked_out_buffer = ring[n];
		ring[n] = locked_out_buffer;

		// Update index to buffer's new position in ring
		ring[n]->index = n;
	}

public:

	CircAcqBuffer()
	{
		ring_size = 0;
		element_size = 0;
	}

	CircAcqBuffer(int number_of_buffers, int frame_size)
	{
		ring_size = number_of_buffers;
		element_size = frame_size;
		head = 0;
		locked.store(-1);
		ring = new CircAcqElement<T>*[ring_size];
		locks = new std::mutex[ring_size];
		for (int i = 0; i < ring_size; i++)
		{
			ring[i] = new(CircAcqElement<T>);
			ring[i]->arr = new T[element_size];
			ring[i]->index = i;
			ring[i]->count = -1;
		}
		// locked_out_buffer maps to actual storage swapped in to replace a buffer when it is locked out
		locked_out_buffer = new(CircAcqElement<T>);
		locked_out_buffer->arr = new T[element_size];
		locked_out_buffer->index = -1;
		locked_out_buffer->count = -1;
		count = 0;
	}

	long lock_out_nowait(unsigned int n, T** buffer)
	{
		if (locked.load() != -1)  // Only one buffer can be locked out at a time
		{
			return -1;
		}
		else
		{
			int requested = mod2(n, ring_size);  // Get index of buffer where requested element is/was
			if (!locks[requested].try_lock())  // Can't lock out/push to same element from two threads at once
			{
				_lock_out(requested);
				locks[requested].release();  // Exit critical section
				*buffer = locked_out_buffer->arr;  // Return pointer to locked out buffer's array
				return locked_out_buffer->count;  // Return n-th buffer you actually got
			}
			else
			{
				return -1;
			}
		}
	}

	long lock_out_wait(unsigned int n, T** buffer)
	{
		while (locked.load() != -1);  // Only one buffer can be locked out at a time
		int requested = mod2(n, ring_size);
		while (!locks[requested].try_lock());
		_lock_out(requested);
		locks[requested].unlock();  // Exit critical section
		*buffer = locked_out_buffer->arr;  // Return pointer to locked out buffer's array
		return locked_out_buffer->count;  // Return n-th buffer you actually got
	}

	void release()
	{
		locked.store(-1);
	}

	int push(T* src)
	{
		while (!locks[head].try_lock());  // prone to deadlock
		memcpy(ring[head]->arr, src, sizeof(T) * element_size);
		count += 1;
		ring[head]->count = count;
		int oldhead = head;
		head = mod2(head + 1, ring_size);
		locks[oldhead].unlock();
		return oldhead;
	}

	// Interface for caller to copy into buffer head directly
	T* lock_out_head()
	{
		while (!locks[head].try_lock());
		return ring[head]->arr;
	}

	int release_head()
	{
		count += 1;
		ring[head]->count = count;
		int oldhead = head;
		head = mod2(head + 1, ring_size);
		locks[oldhead].unlock();
		return oldhead;
	}

	int get_latest_index()
	{
		return count;
	}

	void clear()
	{
		// Reset the buffer to its initial state with a count of 0. Not thread safe
		for (int i = 0; i < ring_size; i++)
		{
			while (!locks[i].try_lock());
			ring[i]->index = i;
			ring[i]->count = -1;
			locks[i].unlock();
		}
		count = 0;
		head = 0;
		locked.store(-1);
		locked_out_buffer->index = -1;
		locked_out_buffer->count = -1;
	}

	~CircAcqBuffer()
	{
		for (int i = 0; i < ring_size; i++)
		{
			delete[] ring[i]->arr;
		}
		delete[] ring;
		delete[] locks;
		delete locked_out_buffer;
	}

};
