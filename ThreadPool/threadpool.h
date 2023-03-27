#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>

// ����������
class Task
{
public:
	// �û������Զ��������������񣬴�Task�̳У���дrun������ʵ���Զ���������
	virtual void run() = 0;
};

// �̳߳�֧�ֵ�ģʽ
enum class PoolMode 
{
	MODE_FIXED, // �̶��������߳�
	MODE_CACHED, // �߳������ɶ�̬����
};

// �߳�����
class Thread
{
public:
	using ThreadFunc = std::function<void()>; // �̺߳�����������

	Thread(ThreadFunc func);
	~Thread();

	// �����߳�
	void start();
private:
	ThreadFunc func_;
};

// �̳߳�����
class ThreadPool
{
public:
	ThreadPool();
	~ThreadPool();

	// �����̳߳صĹ���ģʽ
	void setPoolMode(PoolMode mode);

	// ���ó�ʼ���߳�����
	void setInitThreadSize(int size);

	// ����task�������������ֵ
	void setTaskQueMaxThreshHold(int ThreshHold);

	// ���̳߳��ύ����
	void submitTask(std::shared_ptr<Task> sp);

	// �����̳߳�
	void start(int initThreadSize = 4);

	// û�б�Ҫ���̳߳ؽ��п����͸�ֵ
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
	// �����̺߳���
	void threadFunc();
private:
	//std::vector<Thread*> threads_; // �߳��б�
	std::vector<std::unique_ptr<Thread>> threads_; // �߳��б�
	size_t initThreadSize_; // ��ʼ���߳�����

	std::queue<std::shared_ptr<Task>> taskQue_; // �������
	std::atomic_int taskSize_; // ��������
	int taskQueMaxThreshHold_; // �������������ֵ

	std::mutex taskQueMtx_; // ��֤������е��̰߳�ȫ
	std::condition_variable notFull_; // ��ʾ������в���
	std::condition_variable notEmpty_; // ��ʾ������в���

	PoolMode poolMode_; // ��ǰ�̳߳صĹ���ģʽ
};

#endif