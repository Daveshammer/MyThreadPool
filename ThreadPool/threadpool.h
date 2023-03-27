#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>

// 任务抽象基类
class Task
{
public:
	// 用户可以自定义任意类型任务，从Task继承，重写run方法，实现自定义任务处理
	virtual void run() = 0;
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
	using ThreadFunc = std::function<void()>; // 线程函数对象类型

	Thread(ThreadFunc func);
	~Thread();

	// 启动线程
	void start();
private:
	ThreadFunc func_;
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

	// 设置task任务队列上线阈值
	void setTaskQueMaxThreshHold(int ThreshHold);

	// 给线程池提交任务
	void submitTask(std::shared_ptr<Task> sp);

	// 开启线程池
	void start(int initThreadSize = 4);

	// 没有必要对线程池进行拷贝和赋值
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
	// 定义线程函数
	void threadFunc();
private:
	//std::vector<Thread*> threads_; // 线程列表
	std::vector<std::unique_ptr<Thread>> threads_; // 线程列表
	size_t initThreadSize_; // 初始的线程数量

	std::queue<std::shared_ptr<Task>> taskQue_; // 任务队列
	std::atomic_int taskSize_; // 任务数量
	int taskQueMaxThreshHold_; // 任务队列上线阈值

	std::mutex taskQueMtx_; // 保证任务队列的线程安全
	std::condition_variable notFull_; // 表示任务队列不满
	std::condition_variable notEmpty_; // 表示任务队列不空

	PoolMode poolMode_; // 当前线程池的工作模式
};

#endif