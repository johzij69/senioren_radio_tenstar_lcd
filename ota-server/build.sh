docker buildx build \
  --platform linux/amd64,linux/arm64 \
  -t docker1.prio-ict.local:5001/prio-ict/ota_server:0.0.2 \
  --push \
  .