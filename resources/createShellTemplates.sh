TENANT='your-tenant'
URL="https://$TENANT.cumulocity.com/inventory/managedObjects"
USER='your-user:your-password'
curl -X POST \
  "$URL" \
  -H 'Accept: application/json' \
  --user "$USER" \
  -H 'Content-Type: application/json' \
  -d '{"type":"c8y_DeviceShellTemplate","c8y_Global":{},"deviceType":"c8y_XDKDevice","name":"toggle on/off yellow LED","command":"toggle","category":"operation"}'
  
curl -X POST \
  "$URL" \
  -H 'Accept: application/json' \
  --user "$USER" \
  -H 'Content-Type: application/json' \
  -d '{"type":"c8y_DeviceShellTemplate","c8y_Global":{},"deviceType":"c8y_XDKDevice","name":"enable/disable sensor ... TRUE/FALSE","command":"sensor NOISE TRUE","category":"sensor"}'
  
curl -X POST \
  "$URL" \
  -H 'Accept: application/json' \
  --user "$USER" \
  -H 'Content-Type: application/json' \
  -d '{"type":"c8y_DeviceShellTemplate","c8y_Global":{},"deviceType":"c8y_XDKDevice","name":"change speed","command":"speed 2000","category":"operation"}'
  
curl -X POST \
  "$URL" \
  -H 'Accept: application/json' \
  --user "$USER" \
  -H 'Content-Type: application/json' \
  -d '{"type":"c8y_DeviceShellTemplate","c8y_Global":{},"deviceType":"c8y_XDKDevice","name":"stop publishing","command":"stop","category":"operation"}'
  
curl -X POST \
  "$URL" \
  -H 'Accept: application/json' \
  --user "$USER" \
  -H 'Content-Type: application/json' \
  -d '{"type":"c8y_DeviceShellTemplate","c8y_Global":{},"deviceType":"c8y_XDKDevice","name":"start publishing","command":"start","category":"operation"}'