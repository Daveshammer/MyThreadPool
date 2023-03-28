#include <iostream>
#include <chrono>
#include <thread>
using namespace std;

#include "threadpool.h"

class MyTask : public Task
{
public:
	MyTask(int begin, int end)
		: begin_(begin)
		, end_(end)
	{}
	Any run()
	{
		std::cout << "begin tid:" << std::this_thread::get_id() << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(3));
		unsigned long long sum = 0;
		for (int i = begin_; i <= end_; i++)
			sum += i;
		std::cout << "end tid:" << std::this_thread::get_id() << std::endl;

		return sum;
	}
private:
	int begin_;
	int end_;
};

int main()
{
	{
		ThreadPool pool;

		pool.start(4);

		Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 10000000));
		unsigned long long sum1 = res1.get().cast_<unsigned long long>();
		cout << sum1 << endl;
	}
	cout << "main over!" << endl;

#if 0
	ThreadPool pool;
	// 用户自己设置线程池的工作模式
	pool.setPoolMode(PoolMode::MODE_CACHED);
	pool.start(4);

	Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 10000000));
	Result res2 = pool.submitTask(std::make_shared<MyTask>(10000001, 20000000));
	Result res3 = pool.submitTask(std::make_shared<MyTask>(20000001, 30000000)); 
	pool.submitTask(std::make_shared<MyTask>(20000001, 30000000)); 
	pool.submitTask(std::make_shared<MyTask>(20000001, 30000000)); 
	pool.submitTask(std::make_shared<MyTask>(20000001, 30000000));

	unsigned long long sum1 = res1.get().cast_<unsigned long long>();
	unsigned long long sum2 = res2.get().cast_<unsigned long long>();
	unsigned long long sum3 = res3.get().cast_<unsigned long long>();

	cout << (sum1 + sum2 + sum3) << endl;
#endif

#if 0
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>()); // 提交任务数大于TASK_MAX_THRESHHOLD，可能提交失败
#endif	
	getchar();
}