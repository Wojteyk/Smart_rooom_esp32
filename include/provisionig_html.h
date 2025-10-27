#pragma once

const char html_form[] = R"rawliteral(<!DOCTYPE html>
<html>
<head><title>ESP32 Wi-Fi Setup</title>
    <style>
        body{
            display: flex;
            flex-direction: column;
            justify-content: flex-start; 
            align-items: center;     
            min-height: 100vh;
            margin: 0;
            padding-top: 6vh;
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background-color: rgb(81, 132, 146);
            text-align: center;
        }
        h1{ margin: 0 0 1rem 0; }

        input[id="inputWindow"]{
            margin-top: 10px;
            margin-bottom: 10px;
            border-radius: 1rem;
        }

        form[id="inputBox"]{
            background-color: rgb(32, 115, 154);
            width: 300px;
            padding: 25px; 
            border-radius: 2rem;
            box-sizing: border-box;
        }

        button#connectButton{
            display: block;
            width: 80%;
            padding: 0.6rem 1rem;
            margin: 0.25rem auto 0; /* center horizontally */
            border: none;
            border-radius: 0.75rem;
            background-color: rgb(12, 84, 115);
            color: #fff;
            font-weight: 600;
            cursor: pointer;
        }
        button#connectButton:hover{ opacity: 0.70; }
        button#connectButton:active{ transform: translateY(1px); }
    
    </style>
</head>
<body>
  <h1>Skonfiguruj Wi-Fi</h1>
  <form action="/connect" id="inputBox" method="get">
    SSID (Nazwa sieci):<br>
    <input type="text" id="inputWindow" name="ssid"><br>
    Haslo:<br>
    <input type="password" id="inputWindow" name="password"><br><br>
    <button type="submit" id="connectButton">Połącz</button>
  </form>
</body> 
</html>
)rawliteral";