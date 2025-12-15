import { useState } from 'react';
import { EventLog } from './components/EventLog';
import { ControlPanel } from './components/ControlPanel';
import { NarrativeCard } from './components/NarrativeCard';
import type { ScenarioInfo } from './components/NarrativeCard';
import { useWebSocket } from './context/WebSocketContext';
import { Server, Database, HardDrive, Terminal } from 'lucide-react';

function App() {
  const { isConnected, activeClients } = useWebSocket();
  const [scenarioInfo, setScenarioInfo] = useState<ScenarioInfo>({ 
    title: '', description: '', tips: '', status: 'idle' 
  });

  return (
    <div className="min-h-screen bg-gray-950 text-gray-200 font-sans p-8">
      <header className="mb-8 flex justify-between items-end border-b border-gray-800 pb-4">
        <div>
          <h1 className="text-3xl font-bold bg-gradient-to-r from-blue-400 to-purple-500 bg-clip-text text-transparent">
            DFS Cluster Observability
          </h1>
          <p className="text-gray-500 mt-1">Real-time Distributed File System Monitor</p>
        </div>
        <div className="flex items-center gap-2">
          <span className={`w-3 h-3 rounded-full ${isConnected ? 'bg-green-500 shadow-[0_0_10px_#22c55e]' : 'bg-red-500'}`}></span>
          <span className="text-sm text-gray-400 uppercase tracking-wider font-semibold">
            {isConnected ? 'Live' : 'Disconnected'}
          </span>
        </div>
      </header>

      <main className="grid grid-cols-1 lg:grid-cols-3 gap-6">
        {/* Left Panel: Topology / Stats */}
        <div className="col-span-1 space-y-6">
          <div className="bg-gray-900 p-6 rounded-lg shadow-lg border border-gray-800">
            <h2 className="text-lg font-bold mb-4 flex items-center gap-2 text-gray-100">
              <Server className="text-purple-500" /> Cluster Status
            </h2>
            <div className="grid grid-cols-2 gap-4">
              <div className="bg-gray-800 p-4 rounded flex flex-col items-center">
                <Database className="text-blue-400 mb-2" />
                <span className="text-2xl font-bold">1</span>
                <span className="text-xs text-gray-500 uppercase">Active Server</span>
              </div>
              <div className="bg-gray-800 p-4 rounded flex flex-col items-center">
                <HardDrive className="text-green-400 mb-2" />
                <span className="text-2xl font-bold">{activeClients.length}</span>
                <span className="text-xs text-gray-500 uppercase">Active Clients</span>
              </div>
            </div>
          </div>
          
          {/* Scenario Controller */}
          <ControlPanel onScenarioChange={setScenarioInfo} />
          
          <div className="bg-gray-900 p-6 rounded-lg shadow-lg border border-gray-800">
             <h2 className="text-lg font-bold mb-4 flex items-center gap-2 text-gray-100">
              <Terminal className="text-orange-500" /> Active Clients
            </h2>
            <div className="space-y-2 max-h-[200px] overflow-y-auto scrollbar-thin scrollbar-thumb-gray-700">
              {activeClients.length === 0 && <div className="text-gray-600 italic text-sm">No active clients detected in the last 5 mins.</div>}
              {activeClients.map(client => (
                <div key={client} className="flex items-center gap-2 p-2 bg-gray-800 rounded border border-gray-700 text-xs font-mono break-all">
                  <div className="w-2 h-2 rounded-full bg-green-500"></div>
                  {client}
                </div>
              ))}
            </div>
          </div>
        </div>

        {/* Right Panel: Event Stream + Narrative */}
        <div className="col-span-1 lg:col-span-2 flex flex-col h-full">
          <NarrativeCard info={scenarioInfo} />
          <EventLog />
        </div>
      </main>
    </div>
  );
}

export default App;
