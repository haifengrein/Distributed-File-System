import { useWebSocket } from '../context/WebSocketContext';
import { EventType } from '../context/WebSocketContext';
import { Activity, Lock, FileText, AlertTriangle, CheckCircle, Server, Laptop } from 'lucide-react';

const EventIcon = ({ type }: { type: EventType }) => {
  switch (type) {
    case EventType.LOCK_ACQUIRED: return <Lock className="text-yellow-500" size={16} />;
    case EventType.LOCK_RELEASED: return <Lock className="text-green-500" size={16} />;
    case EventType.LOCK_DENIED: return <AlertTriangle className="text-red-500" size={16} />;
    case EventType.FILE_STORE_START: return <Activity className="text-blue-400" size={16} />;
    case EventType.FILE_STORE_COMPLETE: return <CheckCircle className="text-green-500" size={16} />;
    case EventType.FILE_FETCH: return <FileText className="text-purple-400" size={16} />;
    case EventType.ERROR_OCCURRED: return <AlertTriangle className="text-red-600" size={16} />;
    case EventType.CLIENT_CONNECT: return <Laptop className="text-teal-400" size={16} />;
    default: return <Server className="text-gray-400" size={16} />;
  }
};

export const EventLog = () => {
  const { events, clearEvents } = useWebSocket();

  // Filter out internal system events (Type 999 is our custom Demo Result)
  const displayEvents = events.filter(evt => (evt.type as number) !== 999);

  const formatNode = (node: string) => {
    if (!node) return 'Server';
    if (node === 'server') return 'Server';
    // Map the specific container hostname to a friendly name
    if (node.startsWith('dfs-server-node')) return 'Test Agent';
    
    if (node.length > 15) {
        return `Client-${node.substring(0, 6)}...`;
    }
    return node;
  };

  return (
    <div className="bg-gray-900 text-gray-100 p-4 rounded-xl shadow-xl border border-gray-800 h-[600px] flex flex-col font-mono text-sm">
      <div className="flex justify-between items-center mb-4 border-b border-gray-700 pb-3">
        <h2 className="text-lg font-bold flex items-center gap-2 text-gray-100">
          <Activity className="text-blue-500" /> Event Stream
        </h2>
        <div className="flex items-center gap-2">
          <span className="text-xs text-gray-500 px-2">{displayEvents.length} events</span>
          <button 
            onClick={clearEvents}
            className="px-3 py-1 text-xs bg-gray-800 hover:bg-gray-700 rounded-md text-gray-300 transition border border-gray-700"
          >
            Clear
          </button>
        </div>
      </div>
      
      <div className="flex-1 overflow-y-auto space-y-2 pr-2 scrollbar-thin scrollbar-thumb-gray-700">
        {displayEvents.length === 0 && (
          <div className="h-full flex flex-col items-center justify-center text-gray-600 space-y-2">
            <Activity size={32} className="opacity-20" />
            <span className="text-sm italic">No active events...</span>
          </div>
        )}
        {displayEvents.map((evt, idx) => (
          <div key={idx} className="group flex items-center gap-3 p-2.5 bg-gray-800/30 hover:bg-gray-800 rounded-lg border border-transparent hover:border-gray-700 transition-all">
            <span className="text-gray-600 text-xs w-[70px] font-mono tabular-nums">
              {new Date(evt.timestamp).toLocaleTimeString('en-US', { hour12: false })}
            </span>
            <span className="w-[28px] h-[28px] rounded-full bg-gray-900 flex items-center justify-center border border-gray-800 group-hover:border-gray-600 transition-colors">
              <EventIcon type={evt.type} />
            </span>
            <div className="flex-1 flex items-center justify-between">
              <div className="flex items-center gap-2">
                <span className="text-gray-300 font-medium">
                  {formatNode(evt.source)}
                </span>
                {evt.target && (
                  <>
                    <span className="text-gray-600">→</span>
                    <span className="text-gray-400">{formatNode(evt.target)}</span>
                  </>
                )}
              </div>
              
              {evt.resource && (
                <span className="text-xs bg-blue-500/10 text-blue-400 px-2 py-0.5 rounded border border-blue-500/20 max-w-[150px] truncate">
                  {evt.resource}
                </span>
              )}
            </div>
          </div>
        ))}
      </div>
    </div>
  );
};
