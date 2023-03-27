#include "threadpool.h"

#include <functional>
#include <thread>
#include <iostream>

const int TASK_MAX_THRESHHOLD = 4;

ThreadPool::ThreadPool()
	: initThreadSize_(4)
	, taskSize_(0)
	, taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
	, poolMode_(PoolMode::MODE_CACHED)
{}

ThreadPool::~ThreadPool()
{}

// 设置线程池的工作模式
void ThreadPool::setPoolMode(PoolMode mode)
{
	poolMode_ = mode;
}

// 设置初始的线程数量
void ThreadPool::setInitThreadSize(int size)
{
	initThreadSize_ = size;
}

// 设置task任务队列上线阈值
void ThreadPool::setTaskQueMaxThreshHold(int ThreshHold)
{
	taskQueMaxThreshHold_ = ThreshHold;
}

// 给线程池提交任务		生成任务
void ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	// 获取锁
	std::unique_lock<std::mutex> lock(taskQueMtx_);

	// 线程间通信	等待任务队列有空余
	// 用户提交任务，最长不能阻塞超过1s，否则判断提交任务失败，返回
	if (!notFull_.wait_for(lock, std::chrono::seconds(1), [this] {return taskSize_ < taskQueMaxThreshHold_; }))
	{
		// 表示notFull_等待1s后，条件依然没有满足
		std::cerr << "task queue is full, submit task fail." << std::endl;
		return;
	}

	// 如果有空余，把任务放入任务队列中
	taskQue_.push(sp);
	++taskSize_;

	// 因为新放了任务，任务队列肯定不空了，在notEmpty_上进行通知
	notEmpty_.notify_all();
}

// 开启线程池
void ThreadPool::start(int initThreadSize)
{
	// 记录初始线程个数
	initThreadSize_ = initThreadSize;

	// 创建初始线程
	for (int i = 0; i < initThreadSize_; ++i)
	{
		// 创建线程对象的时候，让线程对象拿到线程函数
		//threads_.push_back(new Thread(std::bind(&ThreadPool::threadFunc, this)));
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this));
		threads_.push_back(std::move(ptr));
	}

	// 启动所有线程 std::vector<Thread*> threads_;
	for (int i = 0; i < initThreadSize_; ++i)
	{
		threads_[i]->start();
	}
}

// 定义线程函数		线程池的所有线程从任务队列里面消费任务
void ThreadPool::threadFunc()
{
	//std::cout << "begin threadFunc tid:" << std::this_thread::get_id() << std::endl;
	//std::cout << "end threadFunc tid:" << std::this_thread::get_id() << std::endl;
	for (;;)
	{
		std::shared_ptr<Task> task;
		{
			// 先获取锁
			std::unique_lock<std::mutex> lock(taskQueMtx_);

			std::cout << "tid:" << std::this_thread::get_id()
				<< "尝试获取任务" << std::endl;

			// 等待notEmpty条件
			notEmpty_.wait(lock, [&]()->bool {return taskQue_.size() > 0; });

			std::cout << "tid:" << std::this_thread::get_id()
				<< "获取任务成功" << std::endl;

			// 从任务队列中取一个任务出来
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;

			// 如果依然有剩余任务，继续通知其它线程执行任务
			if (taskQue_.size() > 0)
			{
				notEmpty_.notify_all();
			}

			// 取出一个任务，就可以通知继续提交生产任务
			notFull_.notify_all();
		} // 取完任务就应该释放锁，然后去执行任务
		// 当前线程负责执行这个任务
		if (task != nullptr)
		{
			task->run();
		}
	}
}

/*
线程方法实现
*/

Thread::Thread(ThreadFunc func)
	: func_(func)
{}

Thread::~Thread()
{}

// 启动线程
void Thread::start()
{
	// 创建一个线程来执行一个线程函数
	std::thread t(func_);
	t.detach();
}