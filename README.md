# CircAcqBuffer
.h c++11 only circular push-only buffer for image streaming

Ring buffer inspired by buffer interface of National Instruments IMAQ software. Elements pushed to the ring are given a count corresponding to the number of times push() has been called since the buffer was initialized. A push() constitutes a copy into buffer-managed memory. The n-th element can be locked out of the ring for processing, copy or display and then subsequently released. If the n-th element isn't available yet, lock_out_nowait() function returns -1 and lock_out_wait() spinlocks until the requested count is available. If the n-th element has been overwritten, the buffer where the n-th element would have been is returned instead along with the count of the element you have actually locked out.

Somewhat thread-safe but only designed for single-producer single-consumer use.
