#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t poolSize) : PoolSize(poolSize) {
    this->workers.reserve(poolSize);
}

void ThreadPool::ThreadFunc() {
    std::unique_ptr<RetraceJob> a = std::move(queue.Pop());
    while (a != nullptr) {
        a->Execute();
        queue.CompleteJob();
        a = std::move(queue.Pop());
    }
}

void ThreadPool::AddJob(std::unique_ptr<RetraceJob> job) {
    queue.Push(std::move(job));
}

void ThreadPool::Join() {
    for (size_t i = 0; i < this->PoolSize; i++) {
        workers.emplace_back(new std::thread([this]() {
            ThreadFunc();
        }));
    }
    for (auto w: workers) {
        w->join();
    }
    workers.clear();
}

void PixelTask(const Scene &scene, Image &image, ViewPlane &viewPlane, Point point, size_t numOfSamples) {
    const auto color = viewPlane.computePixel(scene, point.x, point.y, numOfSamples);
    image.set(point.x, point.y, color);
}


void RetraceJob::Execute() {
    PixelTask(scene, image, viewPlane, cur_point, numOfSamples);
}

RetraceJob::RetraceJob(Scene &scene, Image &image, ViewPlane &viewPlane, Point &curPoint, size_t numOfSamples)
        : scene(scene), viewPlane(viewPlane), image(image), cur_point(curPoint), numOfSamples(numOfSamples) {}

template<typename T>
void ThreadSafeQueue<T>::Push(std::unique_ptr<T> job) {
    std::unique_lock<std::mutex> guard(lock);
    queue.emplace(std::move(job));
    addedJobs++;
    stop = false;
    cond.notify_one();
}

template<typename T>
std::unique_ptr<T> ThreadSafeQueue<T>::Pop() {
    std::unique_lock<std::mutex> guard(lock);
    cond.wait(guard, [&]() { return stop || !queue.empty(); });
    if (stop) {
        return nullptr;
    }

    auto job = std::move(queue.front());
    queue.pop();
    return std::move(job);
}

template<typename T>
void ThreadSafeQueue<T>::Stop() {
    std::unique_lock<std::mutex> guard(lock);
    stop = true;
    cond.notify_all();
}

template<typename T>
void ThreadSafeQueue<T>::CompleteJob() {
    std::unique_lock<std::mutex> guard(lock);
    completedJobs++;
    if (addedJobs == completedJobs) {
        stop = true;
        cond.notify_all();
    }
}
