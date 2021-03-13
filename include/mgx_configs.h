#if !defined(__MGX_CONFIGS_H__)
#define __MGX_CONFIGS_H__

/* Log */
#define CONFIG_LogPathName       "LogPathName"
#define CONFIG_LogLevel          "LogLevel"
#define CONFIG_DebugMode         "DebugMode"

/* Process */
#define CONFIG_WorkerProcesses    "WorkerProcesses"
#define CONFIG_Daemon             "Daemon"
#define CONFIG_WorkerThreadCount  "WorkerThreadCount"

/* Net */
#define CONFIG_ListenPortCount    "ListenPortCount"
#define CONFIG_ListenPort         "ListenPort"
#define CONFIG_WorkerConnections  "WorkerConnections"
#define CONFIG_RecyConnectionWaitTime  "RecyConnectionWaitTime"
#define CONFIG_RecyConnectionWaitTime  "RecyConnectionWaitTime"
#define CONFIG_Heartbeat          "Heartbeat"
#define CONFIG_HeartWaitTime      "HeartWaitTime"

/* Http */
#define CONFIG_IndexPath          "IndexPath"

#endif // __MGX_CONFIGS_H__
