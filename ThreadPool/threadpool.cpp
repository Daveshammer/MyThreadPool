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

// �����̳߳صĹ���ģʽ
void ThreadPool::setPoolMode(PoolMode mode)
{
	poolMode_ = mode;
}

// ���ó�ʼ���߳�����
void ThreadPool::setInitThreadSize(int size)
{
	initThreadSize_ = size;
}

// ����task�������������ֵ
void ThreadPool::setTaskQueMaxThreshHold(int ThreshHold)
{
	taskQueMaxThreshHold_ = ThreshHold;
}

// ���̳߳��ύ����		��������
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	// ��ȡ��
	std::unique_lock<std::mutex> lock(taskQueMtx_);

	// �̼߳�ͨ��	�ȴ���������п���
	// �û��ύ�����������������1s�������ж��ύ����ʧ�ܣ�����
	if (!notFull_.wait_for(lock, std::chrono::seconds(1), [this] {return taskSize_ < taskQueMaxThreshHold_; }))
	{
		// ��ʾnotFull_�ȴ�1s��������Ȼû������
		std::cerr << "task queue is full, submit task fail." << std::endl;
		return Result(sp, false);
	}

	// ����п��࣬������������������
	taskQue_.push(sp);
	++taskSize_;

	// ��Ϊ�·�������������п϶������ˣ���notEmpty_�Ͻ���֪ͨ
	notEmpty_.notify_all();

	return Result(sp);
}

// �����̳߳�
void ThreadPool::start(int initThreadSize)
{
	// ��¼��ʼ�̸߳���
	initThreadSize_ = initThreadSize;

	// ������ʼ�߳�
	for (int i = 0; i < initThreadSize_; ++i)
	{
		// �����̶߳����ʱ�����̶߳����õ��̺߳���
		//threads_.push_back(new Thread(std::bind(&ThreadPool::threadFunc, this)));
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this));
		threads_.push_back(std::move(ptr));
	}

	// ���������߳� std::vector<Thread*> threads_;
	for (int i = 0; i < initThreadSize_; ++i)
	{
		threads_[i]->start();
	}
}

// �����̺߳���		�̳߳ص������̴߳��������������������
void ThreadPool::threadFunc()
{
	//std::cout << "begin threadFunc tid:" << std::this_thread::get_id() << std::endl;
	//std::cout << "end threadFunc tid:" << std::this_thread::get_id() << std::endl;
	for (;;)
	{
		std::shared_ptr<Task> task;
		{
			// �Ȼ�ȡ��
			std::unique_lock<std::mutex> lock(taskQueMtx_);

			std::cout << "tid:" << std::this_thread::get_id()
				<< "���Ի�ȡ����" << std::endl;

			// �ȴ�notEmpty����
			notEmpty_.wait(lock, [&]()->bool {return taskQue_.size() > 0; });

			std::cout << "tid:" << std::this_thread::get_id()
				<< "��ȡ����ɹ�" << std::endl;

			// �����������ȡһ���������
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;

			// �����Ȼ��ʣ�����񣬼���֪ͨ�����߳�ִ������
			if (taskQue_.size() > 0)
			{
				notEmpty_.notify_all();
			}

			// ȡ��һ�����񣬾Ϳ���֪ͨ�����ύ��������
			notFull_.notify_all();
		} // ȡ�������Ӧ���ͷ�����Ȼ��ȥִ������
		// ��ǰ�̸߳���ִ���������
		if (task != nullptr)
		{
			//task->run();
			task->exec();
		}
	}
}

/*
�̷߳���ʵ��
*/

Thread::Thread(ThreadFunc func)
	: func_(func)
{}

Thread::~Thread()
{}

// �����߳�
void Thread::start()
{
	// ����һ���߳���ִ��һ���̺߳���
	std::thread t(func_);
	t.detach();
}

/*
task����ʵ��
*/
Task::Task()
	: result_(nullptr)
{}

void Task::exec()
{
	if (result_ != nullptr)
	{
		result_->setVal(run());
	}
}

void Task::setResult(Result* res)
{
	result_ = res;
}

/*
Result�����ĵ�ʵ��
*/
Result::Result(std::shared_ptr<Task> task, bool isValid)
	: isValid_(isValid)
	, task_(task)
{
	task_->setResult(this);
}

Any Result::get()
{
	if (!isValid_)
	{
		return "";
	}
	sem_.wait(); // task�������û��ִ���꣬����������û����߳�
	return std::move(any_);
}

void Result::setVal(Any any)
{
	// �洢taskִ����ķ���ֵ
	any_ = std::move(any);
	sem_.post(); // �洢��ϣ������ź�����Դ
}
