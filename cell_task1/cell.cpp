#include <string>
#include <iostream>
#include <boost/asio.hpp>
#include "interfaces.hpp"
#include "BillingManager.hpp"
#include "CallManager.hpp"
#include "rmq/BillingManagerAdapter.hpp"
#include "rmq/BillingManagerProxy.hpp"
#include "rmq/CallManagerAdapter.hpp"
#include "rmq/CallManagerProxy.hpp"

void direct(void) {
    boost::asio::io_service svc;
    BillingManager billerImpl(svc);
    CallManager callManagerImpl(svc);
    billerImpl.setBalance("123-123", std::chrono::seconds(5));
    billerImpl.setBalance("321-321", std::chrono::seconds(10));

 
    ICallManager::Call caller1("123-123", callManagerImpl, svc);
    ICallManager::Call caller2("321-321", callManagerImpl, svc);

    billerImpl.connectTo(callManagerImpl);
    callManagerImpl.connectTo(billerImpl);
    caller1.initiate(3);
    caller2.initiate(3);
    svc.run();
}

void broker() {
    boost::asio::io_service svc;
    BillingManager billerImpl(svc);
    CallManager callManagerImpl(svc);
    billerImpl.setBalance("123-123", std::chrono::seconds(5));
    billerImpl.setBalance("321-321", std::chrono::seconds(10));

 
    ICallManager::Call caller1("123-123", callManagerImpl, svc);
    ICallManager::Call caller2("321-321", callManagerImpl, svc);

    rmq::BillingManagerAdapter billerAdapter("amqp://guest:guest@localhost/", billerImpl, svc);
    rmq::BillingManagerProxy   billerProxy("amqp://guest:guest@localhost/", svc);
    billerAdapter.activeRMQ();

    rmq::CallManagerAdapter callManagerAdapter("amqp://guest:guest@localhost/", callManagerImpl, svc);
    rmq::CallManagerProxy   callManagerProxy("amqp://guest:guest@localhost/", svc);
    callManagerAdapter.activeRMQ();

    billerImpl.connectTo(callManagerProxy);
  //  billerImpl.connectTo(callManagerImpl);
    callManagerImpl.connectTo(billerProxy);

    caller1.initiate(3);
    caller2.initiate(3);
    svc.run();
}

/*
CallManager отвечает за обработку звонков на "оборудовании"
BillingManager отвечает за учёт баланса абонентов.
Обе сущности реализуют интерфейсы ICallManager и IBillingManager соответственно.
В данный момент экземпляр CallManager вызывает с методы экземпляра  
BillingManager через rabbit-mq.
Обратное взаимодействие реализовано напрямую. 
Необходимо чтобы BillingManager вызывал CallManager тоже через rabbit-mq.
*/

int main(void)
{
    
   // std::cout << "Direct referencing" << std::endl;
   // direct();
   std::cout << "Broker referencing" << std::endl;
    broker();

    return 1;
}

