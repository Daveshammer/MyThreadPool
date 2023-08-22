#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <unordered_map>

// Any类型：可以接受任意类型数据
class Any
{
public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;

	template<typename T>
	Any(T data) : base_(std::make_unique<Derive<T>>(data))
	{}

	// 这个方法能把Any对象里面存储的data数据提取出来
	template<typename T>
	T cast_()
	{
		// 智能指针 =》 基类指针 =》 派生类指针
		Derive<T>* derive = dynamic_cast<Derive<T>*>(base_.get());
		if (derive == nullptr)
		{
			throw "type is unmatch!";
		}
		return derive->data_;
	}
private:
	class Base
	{
	public:
		virtual ~Base() = default;
	};
	template<typename T>
	class Derive : public Base
	{
	public:
		Derive(T data) : data_(data) {}
		T data_;
	};
private:
	std::unique_ptr<Base> base_;
};

// 实现一个信号量类（精简版条件变量）
class Semaphore 
{
public:
	Semaphore(int limit = 0) : resLimit_(limit) {}
	~Semaphore() = default;
	// 获取资源
	void wait()


	{
		std::unique_lock<std::mutex> lock(mtx_);
		// 如果资源已经被占用完了，就等待
		cond_.wait(lock, [&]()->bool {return resLimit_ > 0; });
		resLimit_--;
	}
	// 释放资源
	void post()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		resLimit_++;
		cond_.notify_all();
	}
private:
	int resLimit_;
	std::mutex mtx_;
	std::condition_variable cond_;
};

// Task类型的前置声明
class Task;

// 实现接受提交到线程池的task任务执行完成后的返回值类型Result
class Result
{
public:
	Result(std::shared_ptr<Task>, bool isValid = true);
	~Result() = default;

	// 存储task执行完的返回值
	void setVal(Any any);

	// 获取task的返回值
	Any get();

private:
	Any any_; // 存储任务的返回值
	Semaphore sem_; // 线程通信信号量
	std::shared_ptr<Task> task_; // 执行对应获取返回值的任务对象，通过强智能指针保证task不会失效
	std::atomic_bool isValid_; // 返回值是否有效
};

// 任务抽象基类
class Task
{
public:
	Task();
	virtual ~Task() = default;
	void exec(); // 执行任务
	void setResult(Result* res);
	// 用户可以自定义任意类型任务，从Task继承，重写run方法，实现自定义任务处理
	virtual Any run() = 0;

private:
	Result* result_; // Result生命周期 > Task
};

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

	Thread(ThreadFunc func);
	~Thread();

	// 启动线程
	void start();

	// 获取线程id
	int getId()const;
private:
	ThreadFunc func_;
	static int generateId_;
	int threadId_; // 保存线程id
};

// 线程池类型
class ThreadPool
{
public:
	ThreadPool();
	~ThreadPool();

	// 设置线程池的工作模式
	void setPoolMode(PoolMode mode);

	// 设置初始的线程数量
	void setInitThreadSize(int size);

	// 设置task任务队列上限阈值
	void setTaskQueMaxThreshHold(int threshHold);

	// 设置线程池cached模式下线程阈值
	void setThreadSizeThreshHold(int threshHold);

	// 给线程池提交任务
	Result submitTask(std::shared_ptr<Task> sp);

	// 开启线程池
	void start(int initThreadSize = std::thread::hardware_concurrency());

	// 没有必要对线程池进行拷贝和赋值
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
	// 定义线程函数
	void threadFunc(int threadid);

	// 检查pool的运行状态
	bool checkRunningState() const;
private:
	//std::vector<Thread*> threads_; // 线程列表
	//std::vector<std::unique_ptr<Thread>> threads_; // 线程列表
	std::unordered_map<int, std::unique_ptr<Thread>> threads_; // 线程列表
	int initThreadSize_; // 初始的线程数量
	std::atomic_int curThreadSize_; // 记录当前线程池里面线程的总数量
	int threadSizeThreshHold_; // 线程数量任务阈值
	std::atomic_int idleThreadSize_; // 记录空闲线程的数量

	std::queue<std::shared_ptr<Task>> taskQue_; // 任务队列，智能指针保证task的生命周期
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