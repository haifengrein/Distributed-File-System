FROM node:20-alpine

WORKDIR /app

# Copy package files
COPY dashboard/package.json dashboard/package-lock.json ./

# Install dependencies
RUN npm install

# Copy source code
COPY dashboard/ ./

# Expose dev server port
EXPOSE 5173

# Start Vite in dev mode
CMD ["npm", "run", "dev"]
