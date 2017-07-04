
#ifndef LIBTASKMANAGER_H
#define LIBTASKMANAGER_H


#include <Arduino.h>



// reset func
typedef void        (*ResetFuncPtr)();
ResetFuncPtr        _resetFunc = NULL;

void reset()
{
	delay(1000);
	_resetFunc();
}


/// checks tha available RAM
int freeRam()
{
    extern int __heap_start, *__brkval;
    int v;
    
    return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}


///< simple task manager class that help split program into small tasks with three priority levels
class TaskManager
{
public:
    enum ePriority
    {
        ETP_HIGH  = 0,
        ETP_NORMAL,
        ETP_LOW,
        ETP_COUNT    ///< number of priority values
    };
    
protected:
    const static int    MAX_TASKS = 8;
    typedef void        (*TaskFunc)();
    
public:
    TaskManager()
    {
        memset(m_uNumTasks, 0, sizeof(size_t) * ETP_COUNT);
        memset(m_uRunCount, 0, sizeof(size_t) * ETP_COUNT);
    }
    
    bool addTask(TaskFunc _fpTask, ePriority _ePriority)
    {
        if (m_uNumTasks[_ePriority] < MAX_TASKS)
        {
            size_t &taskCount = m_uNumTasks[_ePriority];
            m_tasks[_ePriority][taskCount] = _fpTask;
            taskCount++;
            
            return true;  // success
        }
        
        return false; // failed
    }
    
    void run()
    {
        runTask(ETP_HIGH);
        runTask(ETP_HIGH);
        runTask(ETP_NORMAL);
        runTask(ETP_HIGH);
        runTask(ETP_HIGH);
        runTask(ETP_NORMAL);
        runTask(ETP_LOW);
    }
	
private:
    void runTask(ePriority _ePriority)
    {
        if (m_uNumTasks[_ePriority] > 0)
        {
            size_t &taskCount = m_uRunCount[_ePriority];
            m_tasks[_ePriority][taskCount]();
            
            taskCount++;
            if (taskCount >= m_uNumTasks[_ePriority])
            {
                taskCount = 0;
            }
        }
    }
	
private:
    TaskFunc            m_tasks[ETP_COUNT][MAX_TASKS];    ///< high priority tasks
    size_t              m_uNumTasks[ETP_COUNT];
    size_t              m_uRunCount[ETP_COUNT];
};




#endif //#ifndef LIBTASKMANAGER_H

