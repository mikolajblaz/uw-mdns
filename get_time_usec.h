#ifndef GET_TIME_USEC_H
#define GET_TIME_USEC_H

inline uint64_t get_time_usec() {
    struct timeval timer;
    gettimeofday(&timer, NULL);
    return timer.tv_sec * 1000000ULL + timer.tv_usec;
}

#endif	// GET_TIME_USEC_H
