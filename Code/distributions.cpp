#include<random>
#include<cmath>
using namespace std;

/*
    Parent class for all probability distributions used in this program
*/
class ProbabilityDistribution{
    protected:
    random_device rand_dev;
    mt19937 rand_gen;
    public:
    ProbabilityDistribution()
    {
        rand_gen = mt19937(rand_dev());
    }
    virtual double getRandomNumber(){
        return 0.0;
    }
};

/*  
    Uniform probability distribution with parameters start and end 
*/
class UniformDistribution: public ProbabilityDistribution{
    protected:
    uniform_real_distribution<> ung;
    public:
    UniformDistribution(double start,double end){
        ung = uniform_real_distribution<>(start,end);
    }
    double getRandomNumber(){
        return ung(rand_gen);
    }

};

/*
    Constant probability distribution with parameter being a constant value
*/
class ConstantDistribution: public ProbabilityDistribution{
    double const_value;
    public:
    ConstantDistribution(double val){
        const_value = val;
    }
    double getRandomNumber(){
        return const_value;
    }
};
/*
    Exponential probability distribution with parameters as lambda value.
*/
class ExponentialDistribution : public ProbabilityDistribution{
    exponential_distribution<> exp_gn;
    public:
    ExponentialDistribution(double mean_service_time){
        double lambda = 1 / mean_service_time;
        exp_gn = exponential_distribution<>(lambda);
    }
    double getRandomNumber(){
        return exp_gn(rand_gen);
    }
};