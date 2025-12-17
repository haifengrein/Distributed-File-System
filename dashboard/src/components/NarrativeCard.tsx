import { Info } from 'lucide-react';

export interface ScenarioInfo {
  title: string;
  description: string;
  tips: string;
  status: 'idle' | 'running' | 'completed';
}

export const NarrativeCard = ({ info }: { info: ScenarioInfo }) => {
  if (info.status === 'idle') return null;

  return (
    <div className="bg-gray-800/80 border-l-4 border-blue-500 rounded-r-lg p-4 mb-4 animate-in fade-in slide-in-from-top-2 shadow-lg">
      <div className="flex justify-between items-start">
        <div>
          <h3 className="text-blue-400 font-bold text-lg flex items-center gap-2">
            {info.title}
            {info.status === 'running' && (
              <span className="flex h-3 w-3 relative">
                <span className="animate-ping absolute inline-flex h-full w-full rounded-full bg-blue-400 opacity-75"></span>
                <span className="relative inline-flex rounded-full h-3 w-3 bg-blue-500"></span>
              </span>
            )}
          </h3>
          <p className="text-gray-300 mt-1 text-sm">{info.description}</p>
        </div>
      </div>
      
      <div className="mt-3 bg-gray-900/50 p-3 rounded border border-gray-700/50 text-sm flex gap-3 items-start">
        <Info className="text-yellow-500 shrink-0 mt-0.5" size={16} />
        <div>
          <span className="text-yellow-500 font-semibold uppercase text-xs tracking-wide block mb-0.5">
            What to Watch
          </span>
          <span className="text-gray-400">{info.tips}</span>
        </div>
      </div>
    </div>
  );
};
