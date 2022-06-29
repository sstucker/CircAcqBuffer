## CircAcqBuffer

Only tested with MSVC.

Push-only ring buffer inspired by ring buffer interface of National Instruments IMAQ software.

Elements pushed to the ring are given a count.

A push is a copy into buffer-managed memory. Any n-th element can be locked out of the ring for processing, copy or display and then subsequently released.

If the n-th element isn't available yet, is already locked out, or is being accessed by another thread, lock_out() returns -1 after timing out.

If the n-th element has been overwritten, the buffer where the n-th element would have been is returned instead along with the count of the element you have actually locked out.
