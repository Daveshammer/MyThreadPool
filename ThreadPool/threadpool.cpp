#include "threadpool.h"

#include <functional>
#include <thread>
#include <iostream>

const int TASK_MAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 10;
const int THREAD_MAX_IDLE_TIME = 10; // ����10��ͻ��ն�����߳�

ThreadPool::ThreadPool()
	: initThreadSize_(0)
	, taskSize_(0)
	, idleThreadSize_(0)
	, curThreadSize_(0)
	, taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
	, threadSizeThreshHold_(THREAD_MAX_THRESHHOLD)
	, poolMode_(PoolMode::MODE_FIXED)
	, isPoolRunning_(false)
{}

ThreadPool::~ThreadPool()
{}

// �����̳߳صĹ���ģʽ
void ThreadPool::setPoolMode(PoolMode mode)
{
	if (checkRunningState())
		return;
	poolMode_ = mode;
}

// ���ó�ʼ���߳�����
void ThreadPool::setInitThreadSize(int size)
{
	initThreadSize_ = size;
}

// ����task�������������ֵ
void ThreadPool::setTaskQueMaxThreshHold(int threshHold)
{
	if (checkRunningState())
		return;
	taskQueMaxThreshHold_ = threshHold;
}

// �����̳߳�cachedģʽ������ֵ
void ThreadPool::setThreadSizeThreshHold(int threshHold)
{
	if (checkRunningState())
		return;
	if (poolMode_ == PoolMode::MODE_CACHED)
	{
		threadSizeThreshHold_ = threshHold;
	}
}


// ���̳߳��ύ����		��������
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	// ��ȡ��
	std::unique_lock<std::mutex> lock(taskQueMtx_);

	// �̼߳�ͨ��	�ȴ���������п���
	// �û��ύ�����������������1s�������ж��ύ����ʧ�ܣ�����
	if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&] {return taskQue_.size() < taskQueMaxThreshHold_; }))
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

	// cachedģʽ ������ȽϽ��� ������С���������
	if (poolMode_ == PoolMode::MODE_CACHED
		&& taskSize_ > idleThreadSize_
		&& curThreadSize_ < threadSizeThreshHold_)
	{
		std::cout << ">>> create new thread..." << std::endl;

		// �������߳�
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		//threads_.push_back(std::move(ptr));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));		
		// �����߳�
		threads_[threadId]->start();
		// �޸��̸߳�����صı���
		curThreadSize_++;
		idleThreadSize_++;
	}

	return Result(sp);
}

// �����̳߳�
void ThreadPool::start(int initThreadSize)
{
	// �����̳߳ص�����״̬
	isPoolRunning_ = true;

	// ��¼��ʼ�̸߳���
	initThreadSize_ = initThreadSize;
	curThreadSize_ = initThreadSize;

	// ������ʼ�߳�
	for (int i = 0; i < initThreadSize_; ++i)
	{
		// �����̶߳����ʱ�����̶߳����õ��̺߳���
		//threads_.push_back(new Thread(std::bind(&ThreadPool::threadFunc, this)));
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		//threads_.push_back(std::move(ptr));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
	}

	// ���������߳� std::vector<Thread*> threads_;
	for (int i = 0; i < initThreadSize_; ++i)
	{
		threads_[i]->start(); 
		idleThreadSize_++; // ��¼��ʼ�����̵߳�����
	}
}

// �����̺߳���		�̳߳ص������̴߳��������������������
void ThreadPool::threadFunc(int threadid)
{
	auto lastTime = std::chrono::high_resolution_clock().now();
	for (;;)
	{
		std::shared_ptr<Task> task;
		{
			// �Ȼ�ȡ��
			std::unique_lock<std::mutex> lock(taskQueMtx_);

			std::cout << "tid:" << std::this_thread::get_id()
				<< "���Ի�ȡ����" << std::endl;

			// cachedģʽ�£��п����Ѿ������˺ܶ���̣߳����ǿ���ʱ�䳬��60s��Ӧ�û��ն����߳�
			while (taskQue_.size() == 0)
			{
				if (poolMode_ == PoolMode::MODE_CACHED)
				{
					// ������������ʱ������
					if (std::cv_status::timeout ==
						notEmpty_.wait_for(lock, std::chrono::seconds(1)))
					{
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
						if (dur.count() >= THREAD_MAX_IDLE_TIME
							&& curThreadSize_ > initThreadSize_)
						{
							// ��ʼ���յ�ǰ�߳�
							// ��¼�߳���������ر�����ֵ�޸�
							// ���̶߳�����߳��б�������ɾ��   û�а취 threadFunc��=��thread����
							// threadid => thread���� => ɾ��
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
					// �ȴ�notEmpty����
					notEmpty_.wait(lock);
				}

			}
			// �ȴ�notEmpty����
			notEmpty_.wait(lock, [&]()->bool {return taskQue_.size() > 0; });
			idleThreadSize_--; // ���������ˣ������߳�����-1

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
		idleThreadSize_++; // task������ϣ������߳�����+1
		lastTime = std::chrono::high_resolution_clock().now(); // �����߳�ִ���������ʱ��
	}
}

bool ThreadPool::checkRunningState() const
{
	return isPoolRunning_;
}


/*
�̷߳���ʵ��
*/
int Thread::generateId_ = 0;

Thread::Thread(ThreadFunc func)
	: func_(func)
	, threadId_(generateId_++)
{}

Thread::~Thread()
{}

// �����߳�
void Thread::start()
{
	// ����һ���߳���ִ��һ���̺߳���
	std::thread t(func_, threadId_);
	t.detach();
}

int Thread::getId()const
{
	return threadId_;
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
