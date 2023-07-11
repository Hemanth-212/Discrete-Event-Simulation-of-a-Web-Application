#include<iostream>
#include<unordered_set>
#include <fstream>
#include<iomanip>
#include <jsoncpp/json/json.h>
#include "utility.h"

using namespace std;


/* Function to return appropriate random variate generater object*/
void initializeDistributions(ProbabilityDistribution **pd,int pd_type, double pd_time){
    switch(pd_type){

        case CONSTANT_DISTRIBUTION: *pd = new ConstantDistribution(pd_time);
                                    break;
        case UNIFORM_DISTRIBUTION : *pd = new UniformDistribution(0.0,2*pd_time);
                                    break;
        case EXPONENTIAL_DISTRIBUTION: *pd  = new ExponentialDistribution(pd_time);
                                        break;
        default: break;
    }
}

/* Handles arrival events*/
void arrival_handler(Event &event, Eventlist &event_list, Server &web_server,double &simulation_time,double &timeout_minimum,int &request_id,int &drop_rate,ProbabilityDistribution *service_time_gen,ProbabilityDistribution *thinktime_gen,ProbabilityDistribution *timeout_gen){
    
    // Fetch a free thread from the threads available
    Thread th = web_server.td_pool.getFreeThread();

    // No thread is available to handle request
    if(th.thread_id == -1)
    {
        bool add_request = web_server.addRequestBuffer(event);

        // Buffer is full so request drop and create another arrival event
        if(add_request == false)
        {
            ++drop_rate;
            double request_arrival_time = simulation_time + thinktime_gen->getRandomNumber();
            double request_time_out = request_arrival_time + timeout_minimum + timeout_gen->getRandomNumber();
            double request_service_time = service_time_gen->getRandomNumber();
            Event next_event(request_id,-1,-1,ARRIVAL_EVENT,request_arrival_time,request_arrival_time,request_time_out,request_service_time);
            event_list.addEvent(next_event);
            ++request_id;
        }
        else
        {
            // If the request is added to the buffer then a Timeout event is created for that request.
            Event timeout_event(event.request_id,event.thread_id,event.core_id,TIME_OUT,event.time_out_time,event.request_time,event.time_out_time,event.total_service_left);
            event_list.addEvent(timeout_event);
        }

    }
    else
    {
        // Assign current request to the free thread and assign it to a core.
        th.request_id = event.request_id;
        int assign_core_id = web_server.getCoreid();
        event.core_id = assign_core_id;
        event.thread_id = th.thread_id;

        // A timeout event is created for the arrival request.
        Event timeout_event(event.request_id,event.thread_id,event.core_id,TIME_OUT,event.time_out_time,event.request_time,event.time_out_time,event.total_service_left);
        event_list.addEvent(timeout_event);

        // Add the current request to the core runqueue and if core is idle then process this request
        web_server.cores[assign_core_id].addEventToCore(event);
        web_server.cores[assign_core_id].runNextTask(simulation_time,event_list,true);
    }
}

void departure_handler(Event &event, Eventlist &event_list, Server &web_server,unordered_set<int>&completed_requests,vector<double>&v, double &simulation_time,double &timeout_minimum,double &total_response_time,int &request_id,int &delays,int &badput,int &goodput, ProbabilityDistribution *service_time_gen,ProbabilityDistribution *thinktime_gen,ProbabilityDistribution *timeout_gen){
    

    // Make this current core free and schedule and run the next request that is assigned to this core
    web_server.cores[event.core_id].handleDeparture(simulation_time,event,event_list,true);

    // Make this thread free and add it in the available threads
    Thread th = Thread(event.thread_id,-1);
    web_server.td_pool.addThread(th);

    // Check if the request is processed after timeout
    bool timeout = false;
    if(event.time_out_time < event.event_time)
    {
        ++badput;
        timeout = true;
    }
    else
    {
        completed_requests.insert(event.request_id);
        ++goodput;
    }

    // Calculate the response time of the request.
    total_response_time += event.event_time - event.request_time;
    v[delays] = event.event_time - event.request_time;
    ++delays;

    // If there are pending requests in buffer then assign the free thread to the request and assign core
    if(web_server.buffer.isEmpty() == false)
    {
        Event next_request = web_server.getNextRequestBuffer();
        Thread th = web_server.td_pool.getFreeThread();
        th.request_id = next_request.request_id;
        int assign_core_id = web_server.getCoreid();
        next_request.core_id = assign_core_id;
        next_request.thread_id = th.thread_id;
        web_server.cores[assign_core_id].addEventToCore(next_request);
        web_server.cores[assign_core_id].runNextTask(simulation_time,event_list,true);
    }


    /*
        Create an new arrival event if the request has been processed before time out.
        If the request is processed after timeout it does not create an arrival event as time out event has already 
        created an arrival event for this user.
    */
   if(timeout == false)
   {
        double request_arrival_time = simulation_time + thinktime_gen->getRandomNumber();
        double request_time_out = request_arrival_time + timeout_minimum + timeout_gen->getRandomNumber();
        double request_service_time = service_time_gen->getRandomNumber();
        Event next_event(request_id,-1,-1,ARRIVAL_EVENT,request_arrival_time,request_arrival_time,request_time_out,request_service_time);
        event_list.addEvent(next_event);
        ++request_id;
   }

}

/* This function handles the time out events*/
void timeout_handler(Event &event, Eventlist &event_list, Server &web_server,unordered_set<int>&completed_requests,double &simulation_time,double &timeout_minimum,int &request_id,ProbabilityDistribution *service_time_gen,ProbabilityDistribution *thinktime_gen,ProbabilityDistribution *timeout_gen){
    
    // Checks if the request has not been processed yet, if so then it creates an arrival event for this user.
    if(completed_requests.find(event.request_id) == completed_requests.end())
    {
        double request_arrival_time = simulation_time + thinktime_gen->getRandomNumber();
        double request_time_out = request_arrival_time + timeout_minimum + timeout_gen->getRandomNumber();
        double request_service_time = service_time_gen->getRandomNumber();
        Event next_event(request_id,-1,-1,ARRIVAL_EVENT,request_arrival_time,request_arrival_time,request_time_out,request_service_time);
        event_list.addEvent(next_event);
        ++request_id;
    }
}   

/* Handles the time slice completion event, that is the event when a request completes its time slice on cpu*/
void time_slice_expire_handler(Event &event, Eventlist &event_list, Server &web_server,double &simulation_time){

    int assigned_core_id = event.core_id;
    web_server.cores[assigned_core_id].state = false;
    // If the core has requests to process then create a context switch event.
    if(web_server.cores[assigned_core_id].getRunqueueSize() > 0)
    {
        double event_time = simulation_time + web_server.cores[assigned_core_id].context_switch_overhead;
        Event cs_event(event.request_id,event.thread_id,assigned_core_id,CONTEXT_SWITCH,event_time,event.request_time,event.time_out_time,event.total_service_left);
        event_list.addEvent(cs_event);
    }
    // If core does not have any other request to process then schedule this request again.
    else
    {
        web_server.cores[assigned_core_id].addEventToCore(event);
        web_server.cores[assigned_core_id].runNextTask(simulation_time,event_list,false);
    }
}

/* This function handles the context switch event */
void context_switch_handler(Event &event, Eventlist &event_list, Server &web_server,double &simulation_time){

    
    int assigned_core_id = event.core_id;
    
    // Adds the context switched event back into the queue and then schedules the next request to process in round robin scheduling.
    if(web_server.cores[assigned_core_id].scheduling_type == ROUND_ROBIN && event.total_service_left != 0)
    {
       web_server.cores[assigned_core_id].addEventToCore(event);
    }
    web_server.cores[assigned_core_id].runNextTask(simulation_time,event_list,false);
   
}
// Used for printing the event type onto the terminal
string print_event_type(int event_type){
    string st;
   switch (event_type){
        case ARRIVAL_EVENT: st =" Arrival";
                            break;
        case DEPARTURE_EVENT: st =" Departure";
                                break;
        case CONTEXT_SWITCH: st =" Context switch";
                             break;
        case TIME_SLICE_EXPIRE: st =" Time slice expired";
                                break;
        case TIME_OUT: st =" Time out";
                        break;
        default: break;
   }
   return st;
}

int main()
{
    FILE *metricsfile = fopen("metricsfile.txt","a+");
    ifstream file("configurations.json");
    Json::Reader reader;
    Json::Value config;
    reader.parse(file, config);
    
    int i,j,no_of_runs,number_of_cores,no_of_threads,buffer_length,no_of_users,total_delays,schedule_type;
    double cs_overhead,service_time,think_time,timeout_time,quantum_time,timeout_minimum;
    int service_time_pd,timeout_time_pd,thinktime_pd,badput,goodput;

    // Read parameters from the configuration.json file
    no_of_runs = config["Number_of_runs"].asInt();
    number_of_cores = config["Number_of_cores"].asInt();
    no_of_threads = config["Number_of_threads"].asInt();
    buffer_length = config["Buffer_length"].asInt();
    schedule_type = config["Scheduling_type"].asInt();
    quantum_time = config["Quantum_time"].asDouble();
    cs_overhead = config["Context_switch_overhead"].asDouble();
    no_of_users = config["Number_of_users"].asInt() ;
    total_delays = config["Total_delays"].asInt();
    service_time = config["Mean_service_time"].asDouble();
    service_time_pd = config["Service_time_distribution"].asInt();
    think_time = config["Thinktime"].asDouble();
    thinktime_pd = config["Thinktime_distribution"].asInt();
    timeout_time = config["Timeout_time"].asDouble();
    timeout_minimum = config["Timeout_minimum"].asDouble();
    timeout_time_pd = config["Timeout_distribution"].asInt();

    string response_file_name = "responsetime"+to_string(no_of_users)+".txt";
    const char *ptr = response_file_name.c_str();
    FILE *reponsefile = fopen(ptr,"w");
    
    double simulation_time,last_event_time,total_response_time;
    int request_id,delays,drop_rate;
    vector<vector<double>>response_times(no_of_runs,vector<double>(total_delays));
    double overall_throughput=0.0,overall_ser_utlzn=0.0,overall_avg_resp_time=0.0,overall_badput=0.0,overall_goodput=0.0,overall_droprate=0.0,overall_num_in_system=0.0;

    for(i=0;i<no_of_runs;++i)
    {
        /*  
            Initialization of Simulation and system state.
        */
        simulation_time = 0.0;
        last_event_time = 0.0;
        total_response_time=0.0;
        request_id = 0;
        delays = 0;
        badput = 0;
        goodput = 0;
        drop_rate = 0;


        Eventlist event_list;
        Server web_server(number_of_cores,quantum_time,buffer_length,no_of_threads,schedule_type,cs_overhead);
        unordered_set<int>completed_requests;

        // Initialize all random variate generators.
        ProbabilityDistribution *service_time_gen,*timeout_gen,*thinktime_gen;
        initializeDistributions(&service_time_gen,service_time_pd,service_time);
        initializeDistributions(&thinktime_gen,thinktime_pd,think_time);
        initializeDistributions(&timeout_gen,timeout_time_pd,timeout_time);

        // Initialize the event list with all users arrival events 
        for(j=0;j<no_of_users;++j)
        {
            double start_time = simulation_time + thinktime_gen->getRandomNumber();
            double request_time_out = start_time + timeout_minimum+ timeout_gen->getRandomNumber();
            double request_service_time = service_time_gen->getRandomNumber();
            Event event(request_id,-1,-1,ARRIVAL_EVENT,start_time,start_time,request_time_out,request_service_time);
            event_list.addEvent(event);
            ++request_id;
        }
        
        cout<<"------------------------------------------------------------------------------------------------------------"<<endl;
        cout<<setw(15)<<"Simulation time"<<"  | "<<setw(20)<<"Event type"<<"  |"<<setw(10)<<"Request_id"<<"   | "<<setw(20)<<"Service time left"<<"  | "<<setw(10)<<"Core id"<<"  | "<<setw(15)<<"No of cores busy"<<" | "<<endl;
        cout<<"------------------------------------------------------------------------------------------------------------"<<endl;
        // Simulation starts
        while(delays < total_delays)
        {
            Event next_event = event_list.getNextEvent();
            last_event_time = simulation_time;
            simulation_time = next_event.event_time;

            // Update the metrics 
            web_server.updateSystemMetrics(simulation_time,last_event_time);

            cout<<setw(15)<<simulation_time<<"  | "<<setw(20)<<print_event_type(next_event.event_type)<<"  | "<<setw(10)<<next_event.request_id<<"  | "<<setw(20)<<next_event.total_service_left<<"  | "<<setw(10)<<next_event.core_id<<"  | "<<setw(15)<<web_server.getCPUUtilization()*number_of_cores<<"  |"<<endl;
            
            // Call the respective event handler based on the event type.
            switch(next_event.event_type)
            {
                case ARRIVAL_EVENT: arrival_handler(next_event,event_list,web_server,simulation_time,timeout_minimum,request_id,drop_rate,service_time_gen,thinktime_gen,timeout_gen);
                                    break;
                case DEPARTURE_EVENT: departure_handler(next_event,event_list,web_server,completed_requests,response_times[i],simulation_time,timeout_minimum,total_response_time,request_id,delays,badput,goodput,service_time_gen,thinktime_gen,timeout_gen);
                                    break;
                case CONTEXT_SWITCH: context_switch_handler(next_event,event_list,web_server,simulation_time);
                                    break;
                case TIME_SLICE_EXPIRE: time_slice_expire_handler(next_event,event_list,web_server,simulation_time);
                                    break;
                case TIME_OUT:  timeout_handler(next_event,event_list,web_server,completed_requests,simulation_time,timeout_minimum,request_id,service_time_gen,thinktime_gen,timeout_gen);
                                    break;
                default: break;
            }

        }
        cout<<"------------------------------------------------------------------------------------------------------------"<<endl;
        // Printing the metrics on the terminal
        cout<<"Simulation time: "<<simulation_time<<endl;
        cout<<"Throughput: "<<total_delays/simulation_time<<endl;
        cout<<"Server Utilization: "<<web_server.getServerUtilization(simulation_time)<<endl;
        cout<<"Average Response time: "<<total_response_time/total_delays<<endl;
        cout<<"Badput Rate: "<<badput/simulation_time<<endl;
        cout<<"Goodput Rate: "<<goodput/simulation_time<<endl;
        cout<<"Drop rate: "<<drop_rate/simulation_time<<endl;
        cout<<"Average number in system: "<<web_server.getNumberinSystem(simulation_time)<<endl;

        overall_throughput += total_delays/simulation_time;
        overall_ser_utlzn += web_server.getServerUtilization(simulation_time);
        overall_avg_resp_time += total_response_time/total_delays;
        overall_badput += (badput/simulation_time);
        overall_goodput += (goodput/simulation_time);
        overall_droprate += (drop_rate/simulation_time);
        overall_num_in_system += web_server.getNumberinSystem(simulation_time);
        
    }

    // Writing the response time all requests handled in every run into a file.
    fprintf(metricsfile,"%d,%0.7f,%0.7f,%0.7f,%0.7f,%0.7f,%0.7f,%0.7f\n", no_of_users,overall_throughput/no_of_runs,overall_ser_utlzn/no_of_runs,overall_avg_resp_time/no_of_runs,overall_badput/no_of_runs,overall_goodput/no_of_runs,overall_droprate/no_of_runs,overall_num_in_system/no_of_runs);
    for(i=0;i<total_delays;++i)
    {  
        fprintf(reponsefile,"Delay%d,",i);
        for(j=0;j<no_of_runs;++j)
        {
            fprintf(reponsefile,"%0.7f,",response_times[j][i]);
        }
        fprintf(reponsefile,"\n");
    }
    return 0;
}