#ifndef PAGES_H
#define PAGES_H
#endif

constexpr char HTML_PAGE_INDEX[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Настройка Умного Полива</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 20px; background-color: #f4f4f4; color: #333; display: flex; justify-content: center; align-items: center; min-height: 90vh; }
    .container { background-color: #fff; padding: 30px; border-radius: 8px; box-shadow: 0 0 15px rgba(0,0,0,0.2); width: 100%; max-width: 400px; }
    h2 { color: #0056b3; text-align: center; margin-bottom: 20px;}
    label { display: block; margin-bottom: 8px; font-weight: bold; }
    input[type="text"], input[type="password"], input[type="number"] {
      width: calc(100% - 22px); padding: 10px; margin-bottom: 15px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box;
    }
    button { background-color: #5cb85c; color: white; padding: 10px 15px; border: none; border-radius: 4px; cursor: pointer; font-size: 14px; margin-bottom:15px; width:100%; }
    button:hover { background-color: #4cae4c; }
    input[type="submit"] {
      background-color: #0056b3; color: white; padding: 12px 20px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; width: 100%;
    }
    input[type="submit"]:hover { background-color: #004494; }
    .message { padding: 10px; margin-top: 20px; margin-bottom: 0px; border-radius: 4px; text-align: center;}
    .success { background-color: #d4edda; color: #155724; border: 1px solid #c3e6cb; }
    .error { background-color: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }
    a { color: #0056b3; text-decoration: none; }
    a:hover { text-decoration: underline; }
    .footer-link { text-align: center; margin-top: 15px; }
    #location_status { font-size: 0.9em; margin-bottom: 10px; min-height:1.2em; }
  </style>
</head>
<body>
  <div class="container">
    <h2>Настройки Умного Полива</h2>
    <form method="POST" action="/">
      <label for="ssid">WiFi SSID:</label>
      <input type="text" id="ssid" name="ssid" value="%SSID_VAL%">

      <label for="password">WiFi Password:</label>
      <input type="password" id="password" name="password" value="%PASS_VAL%">

      <label for="token">WQTT API Token:</label>
      <input type="text" id="token" name="token" value="%WQTT_TOKEN_VAL%">

      <hr style="margin: 20px 0;">

      <label for="latitude">Широта (Latitude):</label>
      <input type="number" step="any" id="latitude" name="latitude" value="%LATITUDE_VAL%" required>

      <label for="longitude">Долгота (Longitude):</label>
      <input type="number" step="any" id="longitude" name="longitude" value="%LONGITUDE_VAL%" required>

      <hr style="margin: 20px 0;">
      <input type="submit" value="Сохранить">
    </form>
    <button type="button" onclick="confirmReset()" style="background-color: #d9534f; margin-top: 10px;">Сбросить настройки до заводских</button>
    <div id="message-container" style="margin-top: 20px;"></div>
  </div>
  <script>
    function showPosition(position) {
      document.getElementById('latitude').value = position.coords.latitude.toFixed(6);
      document.getElementById('longitude').value = position.coords.longitude.toFixed(6);
      document.getElementById("location_status").innerHTML = "Координаты получены!";
    }

    function showError(error) {
      var statusP = document.getElementById("location_status");
      switch(error.code) {
        case error.PERMISSION_DENIED:
          statusP.innerHTML = "Запрос геолокации отклонен."
          break;
        case error.POSITION_UNAVAILABLE:
          statusP.innerHTML = "Информация о местоположении недоступна."
          break;
        case error.TIMEOUT:
          statusP.innerHTML = "Тайм-аут запроса геолокации."
          break;
        case error.UNKNOWN_ERROR:
          statusP.innerHTML = "Неизвестная ошибка геолокации."
          break;
      }
    }

    function confirmReset() {
      var messageDiv = document.getElementById('message-container');
      messageDiv.innerHTML = '';
      if (confirm("Вы уверены, что хотите сбросить все настройки до заводских? Это действие необратимо и приведет к перезагрузке устройства.")) {
        messageDiv.innerHTML = '<p>Выполняется сброс...</p>';
        fetch('/reset', { method: 'POST' })
          .then(response => {
            if (!response.ok) {
              throw new Error('Network response was not ok: ' + response.statusText);
            }
            return response.text();
          })
          .then(data => {
            messageDiv.innerHTML = '<p class="success">' + data + '</p>';
          })
          .catch(error => {
            console.error('Error during reset:', error);
            messageDiv.innerHTML = '<p class="error">Ошибка при сбросе настроек: ' + error.message + '</p>';
          });
      }
    }
  </script>
</body>
</html>
)rawliteral";
