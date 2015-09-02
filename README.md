# Process-Monitor
API that allows monitoring processes in Windows. Main features:
* allow start and restart process;
* allow to retrieve process info (handle, id, status (is working, restarting,
stopped));
* allow to stop process via method call (without restart) and start it again;
* log all events (start, crash, manual shutdown);
* allow to add callbacks to all events. For example OnProcStart, OnProcCrash, OnProcManuallyStopped;
* all methods must be thread-safe;
* all resources (process handles, threads, file handles, logger, etc.) must
be properly released;
