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
		std::cout << "end tid:" << std::this_thread::get_id() << std::endl;

	}
};

int main()
{
	ThreadPool pool;
	pool.start(4);

	//pool.submitTask(std::make_shared<MyTask>());

	getchar();
}