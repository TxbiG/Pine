
void thread_spawn(void* thread, void* arg);
void thread_join(void* thread);
void thread_detach(void* thread);
void thread_current_id();
void thread_yield();
void thread_sleep_ms(u64 milliseconds);
