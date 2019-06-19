R"rawText(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <meta http-equiv="X-UA-Compatible" content="ie=edge" />
    <link
      rel="stylesheet"
      href="https://use.fontawesome.com/releases/v5.0.13/css/all.css"
      integrity="sha384-DNOHZ68U8hZfKXOrtjWvjxusGo9WQnrNx2sqG0tfsghAvtVlRW3tvkXWZh58N9jp"
      crossorigin="anonymous"
    />
    <link
      rel="stylesheet"
      href="https://stackpath.bootstrapcdn.com/bootstrap/4.1.1/css/bootstrap.min.css"
      integrity="sha384-WskhaSGFgHYWDcbwN70/dfYBj47jz9qbsMId/iRN3ewGhXQFZCSftd1LZCfmhktB"
      crossorigin="anonymous"
    />
    <script src="https://cdn.jsdelivr.net/npm/chart.js@2.8.0"></script>
    
    <title>ESP32 Smart Gardening</title>

    <script type="text/javascript">
      var ws;
      var chart;
      
      function handleCBClick(cb) {
        setTimeout(function() {
          var waterCB = document.getElementById("waterPumpOverrideCheckBox");
          var lampCB = document.getElementById("lampOverrideCheckBox");

          var water, lamp;

          if(waterCB.checked)
            water = 1;
          else
            water = 0;

          if(lampCB.checked)
            lamp = 1;
          else
            lamp = 0;

          var string = '{"waterOverride" : ';
          string += String(water);
          string += ', "lampOverride" : ';
          string += String(lamp);
          string += '}';

          ws.send(string);
          
        }, 0);
      }

      function addDataToChart(chart, label, data) {
        
        chart.data.labels.push(label);
        chart.data.datasets.forEach((dataset) => {
          dataset.data.push(data);
        });
        chart.update();
      }

      function WebSocketBegin() {
        if ("WebSocket" in window) {
          // Let us open a web socket
        ws = new WebSocket(
          location.hostname.match(/\.husarnetusers\.com$/) ? "wss://" + location.hostname + "/__port_8001/" : "ws://" + location.hostname + ":8001/"
            );
          //ws = new WebSocket(
          //  "wss://fc94f91f5992989f83474cc8abf7329b-8001.husarnetusers.com"
          //);

          ws.onopen = function() {
            // Web Socket is connected
          };

          ws.onmessage = function(evt) {
            //create a JSON object
            var jsonObject = JSON.parse(evt.data);
            var jsonMode = jsonObject.jsonMode;
            
            if(jsonMode=="default"){
              var soil = jsonObject.soil;
              var lamp = jsonObject.lamp;
              var water = jsonObject.water;
  
              document.getElementById("soil").innerText = soil;
              
              if (lamp == 1) {
                document.getElementById("lampOverride").style.color = "green";
              } else if (lamp == 0){
                document.getElementById("lampOverride").style.color = "red";
              }
  
              if (water == 1) {
                document.getElementById("waterPumpOverride").style.color = "green";
              } else if (water == 0){
                document.getElementById("waterPumpOverride").style.color = "red";
              }
              
            }else if(jsonMode=="chart"){
              var hour = jsonObject.hour;
              var min = jsonObject.min;
              var reading = jsonObject.reading;

              var time = hour + ":" + min;
              addDataToChart(chart, time, reading);
            }
            
          };

          ws.onclose = function() {
            // websocket is closed.
            alert("Connection is closed...");
          };
        } else {
          // The browser doesn't support WebSocket
          alert("WebSocket NOT supported by your Browser!");
        }
      }
    </script>
  </head>

  <body onLoad="javascript:WebSocketBegin()">
    <header id="main-header" class="py-2 bg-success text-white">
      <div class="container">
        <div class="row justify-content-md-center">
          <div class="col-md-6 text-center">
            <h1>ESP32 Smart Gardening</h1>
          </div>
        </div>
      </div>
    </header>

    <section class="py-5 bg-white">
      <div class="container">
        <div class="row">
          <div class="col">
            <div class="card bg-light m-2" style="min-height: 15rem;">
              <div class="card-header"></div>
              <div class="card-body" align="center">
                <h5 class="card-title">Soil moisture</h5>
                <p class="card-text">
                  Current soil moisture percentage.
                </p>
                <h1 id="soil" class="font-weight-bold">
                  100
                </h1>
              </div>
            </div>
          </div>
        </div>
        <div class="row">
          <div class="col">
            <div class="card bg-light m-2" style="min-height: 15rem;">
              <div class="card-header"></div>
              <div class="card-body">
                <h5 class="card-title">Moisture history</h5>
                <p class="card-text">
                  <canvas id="chart" width="400" height="150"></canvas>
                  <script>
                    var ctx = document.getElementById("chart").getContext('2d');
                    chart = new Chart(ctx, {
                      // The type of chart we want to create
                      type: 'line',
                      // The data for our dataset
                      data: {
                        labels: [],
                        datasets: [{
                          label: 'Moisture history',
                          backgroundColor: 'rgb(92,184,92)',
                          borderColor: 'rgb(92,184,92)',
                          data: []
                        }]
                      },
                      // Configuration options go here
                      options: {
                        scales: {
                          yAxes: [{
                              display: true,
                              ticks: {
                                  beginAtZero: true,   // minimum value will be 0.
                                  max: 100
                              }
                            }]
                          }
                        }
                    });
                  </script>
                </p>
              </div>
            </div>
          </div>
        </div>
        <div class="row">
          <div class="col">
            <div class="card bg-light m-2">
              <div class="card-header"></div>
              <div class="card-body">
                <h5 class="card-title">Water pump</h5>
                <p class="card-text">
                  Water pump current state.
                </p>
                <i id="waterPumpOverride" class="fas fa-circle fa-2x" style="color:red;"></i>
                <div style="float:right" align="center">
                  <p>Override water pump</p>
                  <input type="checkbox" onclick="handleCBClick(this)" class="form-check-input" id="waterPumpOverrideCheckBox">
                </div>
              </div>
            </div>
          </div>
          <div class="col">
            <div class="card bg-light m-2">
              <div class="card-header"></div>
              <div class="card-body">
                <h5 class="card-title">Lamp</h5>
                <p class="card-text">
                  Lamp current state.
                </p>
                  <i id="lampOverride" class="fas fa-circle fa-2x" style="color:red;"></i>
                  <div style="float:right" align="center">
                  <p>Override lamp</p>
                  <input type="checkbox" onclick="handleCBClick(this)" class="form-check-input" id="lampOverrideCheckBox">
                  </div>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>
    </section>
  </body>
</html>

)rawText"
