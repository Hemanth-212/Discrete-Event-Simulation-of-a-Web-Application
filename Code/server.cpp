#include<queue>
#include<vector>
#include<climits>
#include "event.cpp"
#include "code_macros.h"
using namespace std;


/*
    Buffer class is used in two ways in the program.
    1. To represent the buffer which contains the requests that are waiting
       to get a free thread.
    2. Queue of all the threads that are currently active being executed by a core.
*/
class Buffer{
    public:
    queue<Event>buffer;
    int max_buffer;
    Buffer(){

    }
    Buffer(int m_buffer){
        max_buffer = m_buffer;
    }

    bool isEmpty(){
        return buffer.size() == 0;
    }

    bool ifFull(){
        return buffer.size() == max_buffer;
    }

    void addRequest(Event &e){
        buffer.push(e);
    }

    Event getNextRequest(){
        Event e = buffer.front();
        buffer.pop();
        return e;
    }

    int getNumberinBuffer(){
        return buffer.size();
    }

};

/* Class that represents a thread 
    Each thread is identified by its thread_id and request_id contains
    the id of the request this thread is currently handling.
    If it is -1 then the thread is free.
*/
class Thread{
    public:
    int thread_id;
    int request_id;
    Thread(int t_id,int r_id){
        thread_id = t_id;
        request_id = r_id;
    }
    int getThreadId(){
        return thread_id;
    }
    int getRequestId(){
        return request_id;
    }
};

/*
    Threadpool represents all the free threads that are available to handle the requests.
    max_threads is the maximum number of threads the system can have.
    thread_pool is a queue containing the free threads.
*/
class ThreadPool{
    public:
    int max_threads;
    queue<Thread>thread_pool;
    ThreadPool(){

    }
    ThreadPool(int m_threads){
        max_threads = m_threads;
        for(int i=0;i<m_threads;++i)
        {
            thread_pool.push(Thread(i,-1));
        }
    }
    
    /* 
       Returns a thread object.
       If the thread_id is not -1 then a free thread is returned.
    */
    Thread getFreeThread(){
        if(thread_pool.size() == 0)
        {
            return Thread(-1,-1);
        }
        else
        {
            Thread t = thread_pool.front();
            thread_pool.pop();
            return t;
        }
    }
    
    //Adds the free thread object to the threadpool
    void addThread(Thread &t){
        t.request_id = -1;
        thread_pool.push(t);
    }
};

/*
    Represents the core of a web server. It has various data member state of boolean data type if true then core is busy.
    run_queue of type Buffer which contains all requests that are assigned to this core.
*/
class Core{
    public:
    int core_id;
    bool state;
    double time_quantum;
    double context_switch_overhead;
    int scheduling_type;
    Buffer run_queue;
    Core(int c_id,bool s,double t_quantum,int no_of_threads,int schedule_type,double context_overhead){
        core_id = c_id;
        state = s;
        time_quantum = t_quantum;
        context_switch_overhead = context_overhead;
        run_queue.max_buffer = no_of_threads;
        scheduling_type = schedule_type;
    }
    int getRunqueueSize(){
        return run_queue.getNumberinBuffer();
    }
    bool addEventToCore(Event &e){
        if(run_queue.ifFull())
        {
            return false;
        }
        run_queue.addRequest(e);
        return true;
    }
    Event getNextTask(){
        Event e = run_queue.getNextRequest();
        return e;
    }

    //Used to schedule and execute the next request for a core
    void runNextTask(double &simulation_time, Eventlist &et,bool add_context_switch){

        // Core is busy 
        if(state == true)
        {
            return ;
        }

        //Core is idle and has a thread in its runqueue
        if(getRunqueueSize() != 0)
        {
            state = true;
            Event e = getNextTask();

            if(scheduling_type == FCFS)
            {
                double e_time = simulation_time + e.total_service_left + (add_context_switch ? context_switch_overhead:0);
                Event next_event(e.request_id,e.thread_id,e.core_id,DEPARTURE_EVENT,e_time,e.request_time,e.time_out_time,0.0);
                et.addEvent(next_event);
                double context_event_time = e_time + context_switch_overhead;
                Event context_event(e.request_id,-1,e.core_id,CONTEXT_SWITCH,context_event_time,e.request_time,e.time_out_time,0.0);
                et.addEvent(context_event);
            }
            else
            {
                // If total service needed is more than time quantum then create a TIME_SLICE_EXPIRE event.
                if(e.total_service_left > time_quantum)
                {
                    double e_time = simulation_time + time_quantum + (add_context_switch ? context_switch_overhead:0);
                    double r_time = e.total_service_left - time_quantum;
                    Event next_event(e.request_id,e.thread_id,e.core_id,TIME_SLICE_EXPIRE,e_time,e.request_time,e.time_out_time,r_time);
                    et.addEvent(next_event);
                }
                // If total service time needed is less than quantum then create a departure event.
                else
                {
                    double e_time = simulation_time + e.total_service_left + (add_context_switch ? context_switch_overhead:0);
                    Event next_event(e.request_id,e.thread_id,e.core_id,DEPARTURE_EVENT,e_time,e.request_time,e.time_out_time,0.0);
                    et.addEvent(next_event);
                    double context_event_time = e_time + context_switch_overhead;
                    Event context_event(e.request_id,-1,e.core_id,CONTEXT_SWITCH,context_event_time,e.request_time,e.time_out_time,0.0);
                    et.addEvent(context_event);
                }
            }
            
        }

    }   
    // Handles departure of a request from the core.
    void handleDeparture(double &simulation_time, Event &e, Eventlist &et,bool add_context_switch){
        state = false;
        runNextTask(simulation_time,et,add_context_switch);
    }

    
};

class Server{
    public:
    int no_of_cores;
    double server_utilization_area;
    double number_in_system_area;
    vector<Core>cores;
    Buffer buffer;
    ThreadPool td_pool;

    Server(int n_of_cores,double t_quantum,int max_length,int no_of_threads,int schedule_type,double context_switch_overhead){
        no_of_cores = n_of_cores;
        server_utilization_area = 0.0;
        number_in_system_area = 0.0;

        for(int i=0;i<no_of_cores;++i)
        {
            cores.push_back(Core(i,false,t_quantum,no_of_threads,schedule_type,context_switch_overhead));
        }
        buffer.max_buffer = max_length;
        td_pool = ThreadPool(no_of_threads);
    }
    // Returns the core id of the core which has minimum load
    int getCoreid(){
        int c_id = -1;
        int min_load = INT_MAX;
        for(int i=0;i<no_of_cores;++i)
        {
            int core_is_busy = (cores[i].state ? 1:0);

            if(min_load > cores[i].getRunqueueSize() + core_is_busy)
            {
                min_load = cores[i].getRunqueueSize()+core_is_busy;
                c_id = i;
            }
        }
        return c_id;
    }

    // Returns the ratio of cores that are busy.
    double getCPUUtilization(){
        double cnt = 0;
        for(int i=0;i<no_of_cores;++i)
        {
            cnt += (cores[i].state?1:0);
        }
        cnt = cnt / no_of_cores;
        return cnt;
    }

    // Adds a request to buffer and returns true if successfully added or false if dropped.
    bool addRequestBuffer(Event e){
        if(buffer.ifFull())
        {
            return false;
        }
        buffer.addRequest(e);
        return true;
    }

    Event getNextRequestBuffer(){
        return buffer.getNextRequest();
    }

    //Updates the metrics of the system such as the utilization and number in system
    void updateSystemMetrics(double &simulation_time,double &last_event_time){

        double diff_events = simulation_time - last_event_time;
        last_event_time = simulation_time;

        int number_in_cores = 0;
        for(int i=0;i<no_of_cores;++i)
        {
            number_in_cores += (1+cores[i].getRunqueueSize());
        }
        number_in_system_area += (number_in_cores + buffer.getNumberinBuffer())*diff_events;
        double cnt_server_busy = getCPUUtilization();
        server_utilization_area += cnt_server_busy * diff_events;
    }

    // Returns the average utilization of server till the simulation time
    double getServerUtilization(double simulation_time){
        return server_utilization_area / simulation_time;
    }

    // Returns the average number in system till the simulation time
    double getNumberinSystem(double &simulation_time){
        return number_in_system_area/simulation_time;
    }

};


