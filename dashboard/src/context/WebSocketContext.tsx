import { createContext, useContext, useEffect, useState, useMemo, type ReactNode } from 'react';

// Matches C++ EventType enum
export enum EventType {
  UNKNOWN_EVENT = 0,
  CLIENT_CONNECT = 1,
  CLIENT_DISCONNECT = 2,
  FILE_STORE_START = 3,
  FILE_STORE_COMPLETE = 4,
  FILE_FETCH = 5,
  LOCK_ACQUIRED = 6,
  LOCK_DENIED = 7,
  LOCK_RELEASED = 8,
  CONFLICT_DETECTED = 9,
  ERROR_OCCURRED = 10,
}

export interface SystemEvent {
  type: EventType;
  timestamp: number;
  source: string;
  target: string;
  resource: string;
  metadata: Record<string, string>;
}

interface WebSocketContextType {
  isConnected: boolean;
  events: SystemEvent[];
  activeClients: string[];
  clearEvents: () => void;
}

const WebSocketContext = createContext<WebSocketContextType | undefined>(undefined);

export const WebSocketProvider = ({ children }: { children: ReactNode }) => {
  const [isConnected, setIsConnected] = useState(false);
  const [events, setEvents] = useState<SystemEvent[]>([]);

  useEffect(() => {
    // In Docker, localhost:8000 will be forwarded
    const ws = new WebSocket('ws://localhost:8000/ws/events');

    ws.onopen = () => {
      console.log('WebSocket Connected');
      setIsConnected(true);
    };

    ws.onclose = () => {
      console.log('WebSocket Disconnected');
      setIsConnected(false);
    };

    ws.onmessage = (event) => {
      try {
        const data: SystemEvent = JSON.parse(event.data);
        setEvents((prev) => [data, ...prev].slice(0, 1000)); // Keep last 1000
      } catch (err) {
        console.error('Failed to parse event', err);
      }
    };

    return () => {
      ws.close();
    };
  }, []);

  const clearEvents = () => setEvents([]);

  // Calculate active clients based on recent activity
  const activeClients = useMemo(() => {
    const clients = new Set<string>();
    const now = Date.now();
    const ACTIVE_WINDOW_MS = 5 * 60 * 1000; // 5 minutes

    events.forEach(evt => {
      // Consider active if seen in the last 5 minutes
      // Ignore "server" and empty strings
      if (evt.source && evt.source.toLowerCase() !== 'server' && evt.source.trim() !== '') {
        // If event has a timestamp, check it. If not, assume it's new (but our protos have timestamps)
        // The proto timestamp is int64 (ms).
        if (now - evt.timestamp < ACTIVE_WINDOW_MS) {
           clients.add(evt.source);
        }
      }
    });
    return Array.from(clients);
  }, [events]);

  return (
    <WebSocketContext.Provider value={{ isConnected, events, activeClients, clearEvents }}>
      {children}
    </WebSocketContext.Provider>
  );
};

export const useWebSocket = () => {
  const context = useContext(WebSocketContext);
  if (!context) {
    throw new Error('useWebSocket must be used within a WebSocketProvider');
  }
  return context;
};