#include "monitor.h"

FrameQueue::FrameQueue(std::size_t capacity) : capacity_(capacity) {
    pthread_mutex_init(&mutex_, nullptr);
}
FrameQueue::~FrameQueue() { pthread_mutex_destroy(&mutex_); }
void FrameQueue::push(const GloveFrame& frame) {
    pthread_mutex_lock(&mutex_);
    if (frames_.size() == capacity_) {
        frames_.pop_front();
        ++dropped_;
    }
    frames_.push_back(frame);
    pthread_mutex_unlock(&mutex_);
}
bool FrameQueue::tryPop(GloveFrame& frame) {
    pthread_mutex_lock(&mutex_);
    const bool available = !frames_.empty();
    if (available) {
        frame = frames_.front();
        frames_.pop_front();
    }
    pthread_mutex_unlock(&mutex_);
    return available;
}
bool FrameQueue::empty() const {
    pthread_mutex_lock(&mutex_);
    const bool value = frames_.empty();
    pthread_mutex_unlock(&mutex_);
    return value;
}
std::uint64_t FrameQueue::dropped() const {
    pthread_mutex_lock(&mutex_);
    const std::uint64_t value = dropped_;
    pthread_mutex_unlock(&mutex_);
    return value;
}

bool gloveOnline(std::uint64_t now_ns, std::uint64_t last_frame_ns,
                 std::uint64_t timeout_ns) {
    return last_frame_ns != 0 && now_ns >= last_frame_ns &&
           now_ns - last_frame_ns <= timeout_ns;
}
