#include <mutex>
#include <condition_variable>
#include <vector>

using namespace std;


template<typename T>
class MyQueue{

	private:
		const unsigned int capacity = 100;
		vector<T*> queue;
		mutex qMutex;
		condition_variable notEmpty, notFull;

	public:
		void push(T* el){

			unique_lock<mutex> mlock(qMutex);

			while(queue.size() == capacity){
				notFull.wait(mlock);
			}

			queue.push_back(el);
			mlock.unlock();

			notEmpty.notify_all();
		}

		T* get(){

			unique_lock<mutex> mlock(qMutex);

			while(queue.empty()){
				notEmpty.wait(mlock);
			}

			T* el = queue[0];
			queue.erase(queue.begin());
			mlock.unlock();

			notFull.notify_all();

			return el;
		}

		int size(){

			return queue.size();
		}

};