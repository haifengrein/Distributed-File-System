import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';
import App from './App.tsx';
import './index.css';
import { WebSocketProvider } from './context/WebSocketContext.tsx';
import ErrorBoundary from './components/ErrorBoundary.tsx';

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <ErrorBoundary>
      <WebSocketProvider>
        <App />
      </WebSocketProvider>
    </ErrorBoundary>
  </StrictMode>,
);
