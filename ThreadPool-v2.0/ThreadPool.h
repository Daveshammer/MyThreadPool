#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <iostream>
#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <unordered_map>
#include <thread>
#include <future>
#include <chrono>

const int TASK_MAX_THRESHHOLD = 2; // INT32_MAX
const int THREAD_MAX_THRESHHOLD = 10;
const int THREAD_MAX_IDLE_TIME = 10; // 空闲10秒就回收多余的线程

// 线程池支持的模式
enum class PoolMode
{
	MODE_FIXED, // 固定数量的线程
	MODE_CACHED, // 线程数量可动态增长
};

// 线程类型
class Thread
{
public:
	using ThreadFunc = std::function<void(int)>; // 线程函数对象类型

	Thread(ThreadFunc func)
		: func_(func)
		, threadId_(generateId_++)
	{}
	~Thread() = default;

	// 启动线程
	void start()
	{
		// 创建一个线程来执行一个线程函数
		std::thread t(func_, threadId_);
		t.detach();
	}

	int getId()const
	{
		return threadId_;
	}
private:
	ThreadFunc func_;
	static int generateId_;
	int threadId_; // 保存线程id
};

int Thread::generateId_ = 0;

// 线程池类型
class ThreadPool
{
public:
	ThreadPool()
		: initThreadSize_(0)
		, taskSize_(0)
		, idleThreadSize_(0)
		, curThreadSize_(0)
		, taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
		, threadSizeThreshHold_(THREAD_MAX_THRESHHOLD)
		, poolMode_(PoolMode::MODE_FIXED)
		, isPoolRunning_(false)
	{}
	~ThreadPool()
	{
		isPoolRunning_ = false;

		// 等待线程池里面所有的线程返回  有两种状态：阻塞 & 正在执行任务中
		std::unique_lock<std::mutex> lock(taskQueMtx_);
		notEmpty_.notify_all();
		exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
	}

	// 设置线程池的工作模式
	void setPoolMode(PoolMode mode)
	{
		if (checkRunningState())
			return;
		poolMode_ = mode;
	}

	// 设置初始的线程数量
	void setInitThreadSize(int size)
	{
		initThreadSize_ = size;
	}


	// 设置task任务队列上限阈值
	void setTaskQueMaxThreshHold(int threshHold)
	{
		if (checkRunningState())
			return;
		taskQueMaxThreshHold_ = threshHold;
	}

	// 设置线程池cached模式下线程阈值
	void setThreadSizeThreshHold(int threshHold)
	{
		if (checkRunningState())
			return;
		if (poolMode_ == PoolMode::MODE_CACHED)
		{
			threadSizeThreshHold_ = threshHold;
		}
	}

	// 给线程池提交任务
	// 使用可变参模板编程，让submitTask可以接受任意任务函数和任意数量的参数
	template<typename Func, typename... Args>
	auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>
	{
		// 打包任务，放入任务队列里面
		using RType = decltype(func(args...));
		auto task = std::make_shared<std::packaged_task<RType()>>(
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...)
		);
		std::future<RType> result = task->get_future();

		// 获取锁
		std::unique_lock<std::mutex> lock(taskQueMtx_);

		// 线程间通信	等待任务队列有空余
		// 用户提交任务，最长不能阻塞超过1s，否则判断提交任务失败，返回
		if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&] {return taskQue_.size() < taskQueMaxThreshHold_; }))
		{
			// 表示notFull_等待1s后，条件依然没有满足
			std::cerr << "task queue is full, submit task fail." << std::endl;
			auto task = std::make_shared<std::packaged_task<RType()>>(
				[]()->RType {return RType(); }
			);
			(*task)();
			return task->get_future();
		}

		// 如果有空余，把任务放入任务队列中
		//taskQue_.push(sp);
		taskQue_.emplace([task]() {(*task)(); });
		++taskSize_;

		// 因为新放了任务，任务队列肯定不空了，在notEmpty_上进行通知
		notEmpty_.notify_all();

		// cached模式 任务处理比较紧急 场景：小而快的任务
		if (poolMode_ == PoolMode::MODE_CACHED
			&& taskSize_ > idleThreadSize_
			&& curThreadSize_ < threadSizeThreshHold_)
		{
			std::cout << ">>> create new thread..." << std::endl;

			// 创建新线程
			auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			//threads_.push_back(std::move(ptr));
			int threadId = ptr->getId();
			threads_.emplace(threadId, std::move(ptr));
			// 启动线程
			threads_[threadId]->start();
			// 修改线程个数相关的变量
			curThreadSize_++;
			idleThreadSize_++;
		}
		return result;
	}

	// 开启线程池
	void start(int initThreadSize)
	{
		// 设置线程池的运行状态
		isPoolRunning_ = true;

		// 记录初始线程个数
		initThreadSize_ = initThreadSize;
		curThreadSize_ = initThreadSize;

		// 创建初始线程
		for (int i = 0; i < initThreadSize_; ++i)
		{
			// 创建线程对象的时候，让线程对象拿到线程函数
			//threads_.push_back(new Thread(std::bind(&ThreadPool::threadFunc, this)));
			auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			//threads_.push_back(std::move(ptr));
			int threadId = ptr->getId();
			threads_.emplace(threadId, std::move(ptr));
		}

		// 启动所有线程 std::vector<Thread*> threads_;
		for (int i = 0; i < initThreadSize_; ++i)
		{
			threads_[i]->start();
			idleThreadSize_++; // 记录初始空闲线程的数量
		}
	}

	// 没有必要对线程池进行拷贝和赋值
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
	// 定义线程函数
	void threadFunc(int threadid)
	{
		auto lastTime = std::chrono::high_resolution_clock().now();
		// 所有任务必须执行完成，线程池才可以回收所有线程资源
		for (;;)
		{
			Task task;
			{
				// 先获取锁
				std::unique_lock<std::mutex> lock(taskQueMtx_);

				std::cout << "tid:" << std::this_thread::get_id()
					<< "尝试获取任务" << std::endl;

				while (taskQue_.size() == 0)
				{
					// 线程池要结束，回收线程资源
					if (!isPoolRunning_)
					{
						threads_.erase(threadid); // std::this_thread::getid()
						std::cout << "threadid:" << std::this_thread::get_id() << " exit!"
							<< std::endl;
						exitCond_.notify_all();
						return; // 线程函数结束，线程结束
					}
					// cached模式下，有可能已经创建了很多的线程，但是空闲时间超过60s，应该回收多余线程
					if (poolMode_ == PoolMode::MODE_CACHED)
					{
						// 条件变量，超时返回了
						if (std::cv_status::timeout ==
							notEmpty_.wait_for(lock, std::chrono::seconds(1)))
						{
							auto now = std::chrono::high_resolution_clock().now();
							auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
							if (dur.count() >= THREAD_MAX_IDLE_TIME
								&& curThreadSize_ > initThreadSize_)
							{
								// 开始回收当前线程
								// 记录线程数量的相关变量的值修改
								// 把线程对象从线程列表容器中删除   没有办法 threadFunc《=》thread对象
								// threadid => thread对象 => 删除
								threads_.erase(threadid); // std::this_thread::getid()
								curThreadSize_--;
								idleThreadSize_--;

								std::cout << "threadid:" << std::this_thread::get_id() << " exit!"
									<< std::endl;
								return;
							}
						}
					}
					else
					{
						// 等待notEmpty条件
						notEmpty_.wait(lock);
					}

				}

				idleThreadSize_--; // 有任务来了，空闲线程数量-1

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
				//task->run();
				//task->exec();
				task();
			}
			idleThreadSize_++; // task处理完毕，空闲线程数量+1
			lastTime = std::chrono::high_resolution_clock().now(); // 更新线程执行完任务的时间
		}
	}

	// 检查pool的运行状态
	bool checkRunningState() const
	{
		return isPoolRunning_;
	}

private:
	//std::vector<Thread*> threads_; // 线程列表
	//std::vector<std::unique_ptr<Thread>> threads_; // 线程列表
	std::unordered_map<int, std::unique_ptr<Thread>> threads_; // 线程列表
	int initThreadSize_; // 初始的线程数量
	std::atomic_int curThreadSize_; // 记录当前线程池里面线程的总数量
	int threadSizeThreshHold_; // 线程数量任务阈值
	std::atomic_int idleThreadSize_; // 记录空闲线程的数量

	using Task = std::function<void()>;
	std::queue<Task> taskQue_; // 任务队列
	std::atomic_int taskSize_; // 任务数量
	int taskQueMaxThreshHold_; // 任务队列上限阈值

	std::mutex taskQueMtx_; // 保证任务队列的线程安全
	std::condition_variable notFull_; // 表示任务队列不满
	std::condition_variable notEmpty_; // 表示任务队列不空
	std::condition_variable exitCond_; // 等到线程资源全部回收

	PoolMode poolMode_; // 当前线程池的工作模式
	std::atomic_bool isPoolRunning_; // 表示当前线程池的启动状态

};

#endif