#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>

// Any���ͣ����Խ���������������
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
	Any(T data) : base_(std::make_unique<Derive<T>>(data));

	// ��������ܰ�Any��������洢��data������ȡ����
	template<typename T>
	T cast_()
	{
		// ����ָ�� =�� ����ָ�� =�� ������ָ��
		Derive<T>* derive = dynamic_cast<Derive<T>*>(base_.get());
		if ��derive == nullptr)
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
	class Derive
	{
	public:
		Derive(T data) : data_(data) {}
		~Derive() = default;
	private:
		T data_;
	};;
private:
	std::unique_ptr<Base> base_;
};;

// ����������
class Task
{
public:
	// �û������Զ��������������񣬴�Task�̳У���дrun������ʵ���Զ���������
	virtual Any run() = 0;
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