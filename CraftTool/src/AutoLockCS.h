#pragma once
class AutoLockCS
{
    CRITICAL_SECTION *_cs;
public:
    AutoLockCS(CRITICAL_SECTION &cs) {_cs = &cs; EnterCriticalSection(&cs);}
    ~AutoLockCS() {LeaveCriticalSection(_cs);}
};
