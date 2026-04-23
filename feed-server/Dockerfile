FROM node:20-alpine

WORKDIR /app

COPY package*.json ./
RUN npm ci --omit=dev

COPY . .

ENV NODE_ENV=production
ENV PORT=3000
ENV UDP_PORT=6113
ENV DATA_DIR=/app/data

RUN mkdir -p /app/data
VOLUME ["/app/data"]

EXPOSE 3000
EXPOSE 6113/udp

CMD ["npm", "start"]
