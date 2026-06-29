FROM alpine:3.20 AS build

RUN apk add --no-cache cmake g++ make

WORKDIR /app
COPY . .
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  && cmake --build build

FROM alpine:3.20
RUN apk add --no-cache libstdc++
WORKDIR /app
COPY --from=build /app/build/control-plane /app/build/control-plane
COPY --from=build /app/build/node-agent /app/build/node-agent
COPY --from=build /app/build/borgctl /app/build/borgctl
COPY --from=build /app/proto /app/proto
