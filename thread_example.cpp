#include <atomic>
#include <deque>
#include <future>
#include <fstream>
#include <thread>
#include <mutex>
#include <iostream>
#include <vector>
#include <string>


class logger
{
public:
    logger(std::ostream& output) :
        is_running{true},
        output_stream{output},
        log_thread{&logger::log_worker, this} //thread'a su klases metodu. 
    {}

    ~logger()
    {
        is_running.store(false);  // Sustabdom loggeri
        if(log_thread.joinable()) // Jei thread'a galim stabdyt (joinint)
            log_thread.join();    // pabaigiam darbus

       
        while(!messages.empty())
        {
            std::cout << messages.front() << '\n';
            messages.pop_front();
        }
    }

    // Workeris
    void log_worker()
    {
        while(is_running.load()) 
        {
            message_lock.lock();
            if (!messages.empty())
            {
                std::cout << messages.front() << '\n';
                messages.pop_front();
            }
            message_lock.unlock();

            std::this_thread::sleep_for(std::chrono::milliseconds(5)); // galima tobulinti si laukima nes programa  veiks net ir tada kei nereikia
        }
    }

    void write(std::string message)
    {
        std::lock_guard<std::mutex> guard(message_lock); 
        messages.push_back(std::move(message));
    }

private:
    std::deque<std::string> messages;
    std::mutex message_lock;

    std::thread log_thread;
    std::atomic_bool is_running;

    std::ostream& output_stream;
};

// Sukuriam loggeri ir nurodom, kad informacija irasyt turetu i "std::cout"
// tobulinmas galima nurodyti failo streama
static logger my_log(std::cout);

int main(int argc, char* argv[])
{
    my_log.write("Labas, lygiagreti sumavimo programa pradeda darba");

    // Sudarom failu sarasa is kuriu bus duomenys nuskaitomi
    std::vector<std::string> number_files;
    for (int i = 1; i < argc; ++i)
    {
        number_files.push_back(std::string(argv[i]));
    }

    if (number_files.empty())
    {
        my_log.write("Nenurodyti failai is kuriu duomenys bus sudedami");
        return 1;
    }

    my_log.write("Bus sumuojami duomenys is " + std::to_string(number_files.size()) + " failu. Kiekvienas failas bus apdorojamas ant atskiros gijos");

   
    // Rezultato negausim kol gija neatilks savo darbo
    std::vector<std::future<int>> count_futures;
    for (std::string& file : number_files)
    {
        // Sita lambda susumuos visus skaicius viename faile
        auto count_lambda = [](std::string file_name) -> int
        {
            // Atidarom faila, patikrinam ar viskas viekia
            std::ifstream number_file(file_name);
            if (!number_file.is_open() || number_file.bad())
            {
                my_log.write("Failo " + file_name + " nebuvo galima atidaryti. Tai skaiciavimo rezultatu nepakeis");
                return 0;
            }

            // Skaitom faila, sumuojam skaicius
            int result = 0;
            int number_count = 0;
            while(!number_file.eof())
            {
                int temp = 0;
                number_file >> temp;
                result += temp;
                ++number_count;
            }

            my_log.write("Failas " + file_name + " turejo " + std::to_string(number_count) + " skaicius");
            return result;
        };

       
        count_futures.push_back(std::async(std::launch::async, count_lambda, file));
    }

    // Susumuojam kiekvieno failo rezultatus
    int total = 0;
    for (std::future<int>& result : count_futures)
    {
        // result.get() negrazins rezultato kol gija nebaigs darbo.
        total += result.get();
    }

    my_log.write("Visu failu suma: " + std::to_string(total));
    return 0;
}