import { useState, useEffect } from 'react';
import { CheckCircle, XCircle, Loader2, AlertTriangle, Activity, Zap } from 'lucide-react';
import { useWebSocket } from '../context/WebSocketContext';

export const ControlPanel = ({ onScenarioChange }: { onScenarioChange: (info: any) => void }) => {
  const [loading, setLoading] = useState<string | null>(null);
  const [result, setResult] = useState<any>(null);
  const { events } = useWebSocket();

  // Listen for result events from WebSocket (Type 999)
  useEffect(() => {
    if (events.length > 0) {
      const latest = events[0];
      if (Number(latest.type) === 999 && latest.resource === 'ScenarioResult') {
        try {
          const data = JSON.parse(latest.metadata.result_json);
          setResult(data);
          setLoading(null);
        } catch (e) {
          console.error("Failed to parse result", e);
        }
      }
    }
  }, [events]);

  const runScenario = async (type: string) => {
    setLoading(type);
    setResult(null);
    
    // Update Narrative Context
    if (type === 'basic') {
      onScenarioChange({
        title: 'Basic Consistency Check',
        description: 'A standard Store -> Fetch -> Verify cycle to ensure data integrity.',
        tips: 'Look for FILE_STORE followed by FILE_FETCH events. Finally, check the Consistency Card below.',
        status: 'running'
      });
    } else if (type === 'conflict') {
      onScenarioChange({
        title: '🔥 Hotspot Battle (Concurrency)',
        description: 'Two clients aggressively fighting to update "critical_config.json".',
        tips: 'Expect RED "LOCK_DENIED" events! This proves the server is protecting the file from corruption.',
        status: 'running'
      });
    } else if (type === 'performance') {
      onScenarioChange({
        title: '⚡ End-to-End Latency Test',
        description: 'Measuring the full round-trip time for file operations (Client -> Network -> Server Disk -> Ack).',
        tips: 'We will execute 5 sequential Store operations and calculate the average latency.',
        status: 'running'
      });
    }

    const API_BASE = import.meta.env.VITE_API_URL || 'http://localhost:8000';
    try {
        const response = await fetch(`${API_BASE}/api/run/${type}`, { method: 'POST' });
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
    } catch (error) {
      setResult({ status: 'error', message: 'Failed to trigger scenario' });
      setLoading(null);
    }
  };

  // Helper to determine color based on latency
  const getLatencyColor = (ms: number) => {
    if (ms < 20) return 'text-green-400';
    if (ms < 50) return 'text-yellow-400';
    return 'text-red-400';
  };

  return (
    <div className="bg-gray-900 rounded-xl shadow-xl border border-gray-800 overflow-hidden mb-6">
      {/* Header */}
      <div className="bg-gray-800/50 p-4 border-b border-gray-800 flex justify-between items-center">
        <h2 className="text-gray-100 font-bold flex items-center gap-2">
          <Activity className="text-purple-500" size={20} /> Scenario Controller
        </h2>
      </div>

      {/* Actions */}
      <div className="p-4 grid grid-cols-3 gap-3">
        <button
          onClick={() => runScenario('basic')}
          disabled={!!loading}
          className="bg-gray-800 hover:bg-gray-700 border border-gray-700 rounded-lg p-3 flex flex-col items-center gap-2 transition-all group disabled:opacity-50"
        >
          {loading === 'basic' ? <Loader2 className="animate-spin text-blue-500" /> : <CheckCircle className="text-blue-500 group-hover:scale-110 transition-transform" />}
          <span className="text-xs font-semibold text-gray-300">Consistency</span>
        </button>

        <button
          onClick={() => runScenario('conflict')}
          disabled={!!loading}
          className="bg-gray-800 hover:bg-gray-700 border border-gray-700 rounded-lg p-3 flex flex-col items-center gap-2 transition-all group disabled:opacity-50"
        >
          {loading === 'conflict' ? <Loader2 className="animate-spin text-red-500" /> : <AlertTriangle className="text-red-500 group-hover:scale-110 transition-transform" />}
          <span className="text-xs font-semibold text-gray-300">Hotspot Battle</span>
        </button>

        <button
          onClick={() => runScenario('performance')}
          disabled={!!loading}
          className="bg-gray-800 hover:bg-gray-700 border border-gray-700 rounded-lg p-3 flex flex-col items-center gap-2 transition-all group disabled:opacity-50"
        >
          {loading === 'performance' ? <Loader2 className="animate-spin text-yellow-500" /> : <Zap className="text-yellow-500 group-hover:scale-110 transition-transform" />}
          <span className="text-xs font-semibold text-gray-300">Latency Test</span>
        </button>
      </div>

      {/* Result Card */}
      {result && (
        <div className="border-t border-gray-800 p-6 bg-gray-900/50 animate-in fade-in slide-in-from-top-2">
          
          {/* Performance Result UI */}
          {result.type === 'performance' && result.stats ? (
             <div className="flex items-center justify-between">
                <div className="text-center flex-1 border-r border-gray-800">
                    <div className="text-xs text-gray-500 mb-1 uppercase tracking-wider">Avg Latency</div>
                    <div className={`text-3xl font-bold ${getLatencyColor(result.stats.avg)}`}>
                        {result.stats.avg}<span className="text-sm text-gray-600 ml-1">ms</span>
                    </div>
                </div>
                <div className="text-center flex-1 border-r border-gray-800">
                    <div className="text-xs text-gray-500 mb-1 uppercase tracking-wider">Max Jitter</div>
                    <div className="text-xl font-mono text-gray-300">
                        {result.stats.max}<span className="text-xs text-gray-600 ml-1">ms</span>
                    </div>
                </div>
                <div className="text-center flex-1">
                    <div className="text-xs text-gray-500 mb-1 uppercase tracking-wider">Samples</div>
                    <div className="text-xl font-mono text-blue-400">
                        {result.stats.samples}
                    </div>
                </div>
             </div>
          ) : (
            /* Consistency Result UI */
            <>
              <div className={`flex items-center justify-center gap-2 mb-4 text-lg font-bold ${result.status === 'success' ? 'text-green-400' : 'text-red-400'}`}>
                {result.status === 'success' ? <CheckCircle size={20} /> : <XCircle size={20} />}
                {result.message}
              </div>

              {(result.client_hash || result.server_hash) && (
                <div className="grid grid-cols-7 gap-2 items-center text-xs font-mono">
                  <div className="col-span-3 bg-gray-800 p-2 rounded text-center border border-gray-700">
                    <div className="text-gray-500 mb-1">Client Hash</div>
                    <div className="text-blue-300 truncate">{result.client_hash}</div>
                  </div>
                  <div className="col-span-1 flex justify-center text-gray-600">=</div>
                  <div className="col-span-3 bg-gray-800 p-2 rounded text-center border border-gray-700">
                    <div className="text-gray-500 mb-1">Server Hash</div>
                    <div className={`truncate ${result.status === 'success' ? 'text-green-300' : 'text-red-300'}`}>
                      {result.server_hash}
                    </div>
                  </div>
                </div>
              )}
            </>
          )}
        </div>
      )}
    </div>
  );
};
