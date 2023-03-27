#include <iostream>
#include <chrono>
#include <thread>
using namespace std;

#include "threadpool.h"

class MyTask : public Task
{
public:
	void run()
	{
		std::cout << "begin tid:" << std::this_thread::get_id() << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(5));
		std::cout << "end tid:" << std::this_thread::get_id() << std::endl;

	}
};

int main()
{
	ThreadPool pool;
	pool.start(4);

	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>()); // 提交任务数大于TASK_MAX_THRESHHOLD，可能提交失败
	
	getchar();
}