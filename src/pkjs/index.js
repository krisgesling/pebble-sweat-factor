// Sweat Factor - PebbleKit JS companion
// Fetches dew point, temperature, and rain probability from Open-Meteo
// Sends comfort label, comfort level, rain ETA, and rain urgency to watch

var FETCH_INTERVAL_NORMAL  = 15 * 60 * 1000; // 15 minutes
var FETCH_INTERVAL_URGENT  = 5  * 60 * 1000; // 5 minutes (rain within 60 min)
var fetchTimer = null;

// ----- Mock location (set to non-null to override GPS for testing) -----
var MOCK_LOCATION = null;
// Bangkok = { lat: 13.7563, lon: 100.5018 }
// Brisbane = { lat: -27.4678, lon: 153.0280 }
// Cairns =  = { lat: -16.9186, lon: 145.7781 }
// Singapore = { lat: 1.3521, lon: 103.8198 }
// Kuala Lumpur = { lat: 3.1390, lon: 101.6869 }
// Manaus, Brazil = { lat: -3.1190, lon: -60.0217 }
// HCM City = { lat: 10.8231, lon: 106.6297 }
// Katherine = { lat: -14.46517, lon: 132.26347 }
// Denpasar = { lat: -8.65, lon: 115.21667 }
// Port Moresby = { lat: -9.44314, lon: 147.17972 }
// Miami = { lat: 25.77427, lon: -80.19366 }
// var MOCK_LOCATION = { lat: 1.3521, lon: 103.8198 }

// ----- Mock scenarios (set MOCK_SCENARIO to a key below to bypass weather fetch) -----
var MOCK_SCENARIOS = {
  // Comfort levels — all clear
  'no-dramas':      { COMFORT_LABEL: 'No dramas', COMFORT_LEVEL: 0, COMFORT_TEMPS: '24\u00B0C \u00B7 feels 25\u00B0C', RAIN_ETA: 'All clear',          RAIN_URGENT: 0, DEW_POINT: 'dp 15\u00B0' },
  'sweating':       { COMFORT_LABEL: 'Sweating',  COMFORT_LEVEL: 1, COMFORT_TEMPS: '28\u00B0C \u00B7 feels 30\u00B0C', RAIN_ETA: 'All clear',          RAIN_URGENT: 0, DEW_POINT: 'dp 19\u00B0' },
  'crikey':         { COMFORT_LABEL: 'Crikey',    COMFORT_LEVEL: 1, COMFORT_TEMPS: '30\u00B0C \u00B7 feels 33\u00B0C', RAIN_ETA: 'All clear',          RAIN_URGENT: 0, DEW_POINT: 'dp 22\u00B0' },
  'cooked':         { COMFORT_LABEL: 'Cooked',    COMFORT_LEVEL: 2, COMFORT_TEMPS: '33\u00B0C \u00B7 feels 37\u00B0C', RAIN_ETA: 'All clear',          RAIN_URGENT: 0, DEW_POINT: 'dp 26\u00B0' },
  // Rain scenarios
  'rain-2pm':       { COMFORT_LABEL: 'Sweating',  COMFORT_LEVEL: 1, COMFORT_TEMPS: '29\u00B0C \u00B7 feels 31\u00B0C', RAIN_ETA: 'Rain ~2:00pm',       RAIN_URGENT: 0, DEW_POINT: 'dp 20\u00B0' },
  'rain-overnight': { COMFORT_LABEL: 'Crikey',    COMFORT_LEVEL: 1, COMFORT_TEMPS: '31\u00B0C \u00B7 feels 34\u00B0C', RAIN_ETA: 'Overnight ~3:00am',  RAIN_URGENT: 0, DEW_POINT: 'dp 23\u00B0' },
  'storm':          { COMFORT_LABEL: 'Cooked',    COMFORT_LEVEL: 2, COMFORT_TEMPS: '34\u00B0C \u00B7 feels 38\u00B0C', RAIN_ETA: 'Storm in ~5min',     RAIN_URGENT: 1, DEW_POINT: 'dp 27\u00B0' },
};
var MOCK_SCENARIO = null; // e.g. 'crikey' or 'storm' — null to use real weather

// ----- HTTP helper -----
function xhrRequest(url, callback, errorCallback) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function() {
    if (this.status >= 200 && this.status < 300) {
      callback(this.responseText);
    } else {
      if (errorCallback) errorCallback('HTTP ' + this.status);
    }
  };
  xhr.onerror = function() {
    if (errorCallback) errorCallback('Network error');
  };
  xhr.open('GET', url);
  xhr.send();
}

// ----- Configurable level names -----
var DEFAULT_LABELS = ['No dramas', 'Sweating', 'Crikey', 'Cooked'];

function getLabels() {
  return DEFAULT_LABELS.map(function(def, i) {
    return localStorage.getItem('sf_label_' + i) || def;
  });
}

// ----- Comfort label logic -----
function getComfortInfo(dewPointC, tempC, feelsLikeC) {
  var labels = getLabels();
  var label, level;
  if (dewPointC < 18) {
    label = labels[0]; level = 0;
  } else if (dewPointC < 21) {
    label = labels[1]; level = 1;
  } else if (dewPointC < 24) {
    label = labels[2]; level = 1;
  } else {
    label = labels[3]; level = 2;
  }
  var temps = Math.round(tempC) + '\u00B0C \u00B7 feels ' + Math.round(feelsLikeC) + '\u00B0C';
  return { label: label, temps: temps, level: level };
}

// ----- Configuration page -----
function buildConfigHTML() {
  var labels = getLabels();
  var colors  = ['#66CDAA', '#FFAA00', '#FFAA00', '#FF8800'];
  var ranges  = ['Below 18\u00B0 dew', '18\u201321\u00B0 dew',
                 '21\u201324\u00B0 dew', '24\u00B0+ dew'];
  var rows = '';
  for (var i = 0; i < 4; i++) {
    rows += '<div class="row">' +
      '<label style="color:' + colors[i] + '">' + ranges[i] + '</label>' +
      '<input type="text" name="l' + i + '" value="' +
        labels[i].replace(/"/g, '&quot;') + '" maxlength="14">' +
      '</div>';
  }
  return '<!DOCTYPE html><html><head><meta charset="utf-8">' +
    '<meta name="viewport" content="width=device-width,initial-scale=1">' +
    '<style>' +
    'body{background:#000;color:#fff;font-family:sans-serif;padding:20px;margin:0}' +
    'h2{margin:0 0 4px;font-size:18px}' +
    'p{margin:0 0 20px;font-size:12px;color:#666}' +
    '.row{margin-bottom:14px}' +
    'label{display:block;font-size:12px;font-weight:bold;margin-bottom:4px}' +
    'input{width:100%;box-sizing:border-box;background:#1a1a1a;color:#fff;' +
      'border:1px solid #444;border-radius:4px;padding:10px;font-size:16px}' +
    'button{width:100%;padding:12px;border:none;border-radius:4px;' +
      'font-size:16px;cursor:pointer;margin-top:8px}' +
    '.save{background:#66CDAA;color:#000;font-weight:bold}' +
    '.reset{background:#222;color:#888}' +
    '</style></head><body>' +
    '<h2>Sweat Factor</h2><p>Comfort level names (max 14 chars)</p>' +
    rows +
    '<button class="save" onclick="save()">Save</button>' +
    '<button class="reset" onclick="resetDefaults()">Reset to defaults</button>' +
    '<script>' +
    'var defs=' + JSON.stringify(DEFAULT_LABELS) + ';' +
    'function save(){' +
      'var d={};' +
      'for(var i=0;i<4;i++){' +
        'var v=document.querySelector("[name=l"+i+"]").value.trim();' +
        'd["label"+i]=v||defs[i];' +
      '}' +
      'location.href="pebblejs://close?"+encodeURIComponent(JSON.stringify(d));' +
    '}' +
    'function resetDefaults(){' +
      'for(var i=0;i<4;i++)document.querySelector("[name=l"+i+"]").value=defs[i];' +
    '}' +
    '</script></body></html>';
}

Pebble.addEventListener('showConfiguration', function() {
  Pebble.openURL('data:text/html,' + encodeURIComponent(buildConfigHTML()));
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e.response || e.response === 'CANCELLED') return;
  try {
    var config = JSON.parse(decodeURIComponent(e.response));
    for (var i = 0; i < 4; i++) {
      var val = config['label' + i];
      if (val) localStorage.setItem('sf_label_' + i, val);
    }
    console.log('Sweat Factor:labels saved, refreshing weather');
    getWeather();
  } catch (err) {
    console.log('Sweat Factor:config parse error: ' + err);
  }
});

// ----- Weather type from WMO hourly code -----
function getWeatherType(code) {
  if (code >= 95) return 'Storm';
  if (code >= 51) return 'Rain';   // rain, showers, drizzle all = Rain in tropical context
  return 'Rain';
}

// ----- Parse Open-Meteo timestamp to local Date -----
// Input: "YYYY-MM-DDTHH:MM"
function parseHourlyTime(str) {
  var d = str.split('T')[0].split('-');
  var t = str.split('T')[1].split(':');
  return new Date(+d[0], +d[1] - 1, +d[2], +t[0], +t[1], 0, 0);
}

// ----- Format a local clock time as "4:30pm" -----
function formatTime(date) {
  var h = date.getHours();
  var m = date.getMinutes();
  var ampm = h >= 12 ? 'pm' : 'am';
  var dh = h % 12 || 12;
  var dm = m < 10 ? '0' + m : '' + m;
  return dh + ':' + dm + ampm;
}

// ----- Rain ETA label -----
// rainDate: Date of first forecast rain, or null for none found
// weatherCode: WMO code at that hour
// isNow: currently precipitating
function getRainEta(rainDate, weatherCode, isNow) {
  if (isNow) {
    return { text: 'Raining now', urgent: 1 };
  }
  if (!rainDate) {
    return { text: 'All clear', urgent: 0 };
  }

  var now = new Date();
  var rainMinutes = (rainDate.getTime() - now.getTime()) / 60000;
  var type = getWeatherType(weatherCode);

  // Under an hour — relative time
  if (rainMinutes < 60) {
    var mins = Math.max(1, Math.round(rainMinutes));
    return { text: type + ' in ~' + mins + 'min', urgent: rainMinutes <= 20 ? 1 : 0 };
  }

  var timeStr = formatTime(rainDate);

  // Day comparison (calendar days, local time)
  var nowMidnight  = new Date(now.getFullYear(),      now.getMonth(),      now.getDate());
  var rainMidnight = new Date(rainDate.getFullYear(), rainDate.getMonth(), rainDate.getDate());
  var dayDiff = Math.round((rainMidnight - nowMidnight) / 86400000);
  var rainHour = rainDate.getHours();

  if (dayDiff === 0) {
    // Same calendar day
    if (rainHour >= 20) return { text: 'Tonight ~' + timeStr, urgent: 0 };
    return { text: type + ' ~' + timeStr, urgent: 0 };
  }
  if (dayDiff === 1) {
    // Next calendar day — wee hours feel like "tonight", rest is tomorrow
    if (rainHour < 6) return { text: 'Overnight ~' + timeStr, urgent: 0 };
    return { text: 'Tomorrow ~' + timeStr, urgent: 0 };
  }
  // Shouldn't reach here in a 48h scan, but just in case
  return { text: 'All clear', urgent: 0 };
}

// ----- Find current hour index in Open-Meteo hourly array -----
// Timestamps are local-time strings like "2024-04-06T14:00"
function findCurrentHourIndex(times) {
  var now = new Date();
  var nowDateStr = now.getFullYear() + '-' +
      pad2(now.getMonth() + 1) + '-' +
      pad2(now.getDate());
  var nowHour = now.getHours();
  var target = nowDateStr + 'T' + pad2(nowHour) + ':00';

  // Find exact match first
  for (var i = 0; i < times.length; i++) {
    if (times[i] === target) return i;
  }
  // Fallback: find closest past hour
  for (var i = times.length - 1; i >= 0; i--) {
    if (times[i] <= target) return i;
  }
  return 0;
}

function pad2(n) {
  return n < 10 ? '0' + n : '' + n;
}

// ----- Schedule next fetch -----
function scheduleNextFetch(rainMinutes) {
  if (fetchTimer) clearTimeout(fetchTimer);
  var interval = (rainMinutes >= 0 && rainMinutes <= 60)
      ? FETCH_INTERVAL_URGENT
      : FETCH_INTERVAL_NORMAL;
  fetchTimer = setTimeout(getWeather, interval);
  console.log('Sweat Factor:next fetch in ' + (interval / 60000) + ' min');
}

// ----- Send error state to watch -----
function sendError(reason) {
  console.log('Sweat Factor:sending error state — ' + reason);
  try {
    Pebble.sendAppMessage(
      {
        'COMFORT_LABEL': 'No data',
        'COMFORT_LEVEL': 0,
        'COMFORT_TEMPS': '',
        'RAIN_ETA': 'No data',
        'RAIN_URGENT': 0
      },
      function() { console.log('Sweat Factor:error state sent'); },
      function(e) { console.log('Sweat Factor:failed to send error state: ' + JSON.stringify(e)); }
    );
  } catch (err) {
    console.log('Sweat Factor:exception in sendError: ' + err);
  }
}

// ----- Main fetch function -----
function fetchWeather(lat, lon) {
  // Override with mock location if set
  if (MOCK_LOCATION) {
    lat = MOCK_LOCATION.lat;
    lon = MOCK_LOCATION.lon;
    console.log('Sweat Factor:using mock location lat=' + lat + ' lon=' + lon);
  }
  var url = 'https://api.open-meteo.com/v1/forecast' +
      '?latitude=' + lat +
      '&longitude=' + lon +
      '&hourly=precipitation_probability,dew_point_2m,temperature_2m,apparent_temperature,weather_code' +
      '&current=precipitation,weather_code' +
      '&forecast_days=2' +
      '&timezone=auto';

  console.log('Sweat Factor:fetching from Open-Meteo lat=' + lat + ' lon=' + lon);

  xhrRequest(url, function(responseText) {
    try {
      var data = JSON.parse(responseText);
      var hourly = data.hourly;
      var times  = hourly.time;
      var prec    = hourly.precipitation_probability;
      var dew     = hourly.dew_point_2m;
      var temp    = hourly.temperature_2m;
      var feels   = hourly.apparent_temperature;
      var codes   = hourly.weather_code;

      var idx = findCurrentHourIndex(times);
      console.log('Sweat Factor: current hour index=' + idx + ' time=' + times[idx]);

      var dewPoint    = dew[idx];
      var temperature = temp[idx];
      var feelsLike   = feels[idx];

      // Check if actively raining now
      var currentPrecip = (data.current && data.current.precipitation > 0);
      if (currentPrecip) {
        console.log('Sweat Factor: raining now (' + data.current.precipitation + 'mm)');
      }

      // Scan up to 48 hours ahead for prob > 50%
      var firstRainDate = null;
      var firstRainCode = 61; // default to plain rain
      if (!currentPrecip) {
        for (var h = 1; h <= 48; h++) {
          var scanIdx = idx + h;
          if (scanIdx >= times.length) break;
          if (prec[scanIdx] > 50) {
            firstRainDate = parseHourlyTime(times[scanIdx]);
            firstRainCode = codes[scanIdx] || 61;
            var rainMinutes = (firstRainDate.getTime() - Date.now()) / 60000;
            console.log('Sweat Factor: rain at ' + times[scanIdx] +
              ' (~' + Math.round(rainMinutes) + 'min, prob=' + prec[scanIdx] +
              '%, code=' + firstRainCode + ')');
            break;
          }
        }
      }

      var comfort = getComfortInfo(dewPoint, temperature, feelsLike);
      var rain    = getRainEta(firstRainDate, firstRainCode, currentPrecip);
      var rainMinutesForSchedule = firstRainDate
          ? (firstRainDate.getTime() - Date.now()) / 60000
          : -1;

      console.log('Sweat Factor:' + comfort.label + ' | ' + rain.text);

      Pebble.sendAppMessage(
        {
          'COMFORT_LABEL': comfort.label,
          'COMFORT_LEVEL': comfort.level,
          'COMFORT_TEMPS': comfort.temps,
          'RAIN_ETA':      rain.text,
          'RAIN_URGENT':   rain.urgent,
          'DEW_POINT':     'dp ' + Math.round(dewPoint) + '\u00B0'
        },
        function() { console.log('Sweat Factor:data sent to watch'); },
        function(e) { console.log('Sweat Factor:send failed: ' + JSON.stringify(e)); }
      );

      scheduleNextFetch(rainMinutesForSchedule);

    } catch (err) {
      console.log('Sweat Factor:parse error: ' + err);
      sendError('parse error');
      scheduleNextFetch(-1);
    }
  }, function(err) {
    console.log('Sweat Factor:fetch error: ' + err);
    sendError(err);
    scheduleNextFetch(-1);
  });
}

// ----- Geolocation -----
function getWeather() {
  if (MOCK_SCENARIO) {
    var mock = MOCK_SCENARIOS[MOCK_SCENARIO];
    if (mock) {
      console.log('Sweat Factor:using mock scenario "' + MOCK_SCENARIO + '"');
      Pebble.sendAppMessage(mock,
        function() { console.log('Sweat Factor:mock data sent'); },
        function(e) { console.log('Sweat Factor:mock send failed: ' + JSON.stringify(e)); }
      );
      return;
    }
    console.log('Sweat Factor:unknown mock scenario "' + MOCK_SCENARIO + '", falling through to real weather');
  }

  // Try to get fresh position; fall back to cached coords if geolocation fails
  navigator.geolocation.getCurrentPosition(
    function(pos) {
      var lat = pos.coords.latitude;
      var lon = pos.coords.longitude;
      // Cache for fallback
      try {
        localStorage.setItem('sf_lat', lat);
        localStorage.setItem('sf_lon', lon);
      } catch (e) {}
      fetchWeather(lat, lon);
    },
    function(err) {
      console.log('Sweat Factor:geolocation error (' + err.code + '): ' + err.message);
      // Try cached coordinates
      var cachedLat = null;
      var cachedLon = null;
      try {
        cachedLat = localStorage.getItem('sf_lat');
        cachedLon = localStorage.getItem('sf_lon');
      } catch (e) {}

      if (cachedLat && cachedLon) {
        console.log('Sweat Factor:using cached coords lat=' + cachedLat + ' lon=' + cachedLon);
        fetchWeather(parseFloat(cachedLat), parseFloat(cachedLon));
      } else if (err.code === 1) {
        // Permission denied — can't do anything without location
        console.log('Sweat Factor:location permission denied, no cached coords');
        sendError('Location denied');
        scheduleNextFetch(-1);
      } else {
        sendError('No location');
        scheduleNextFetch(-1);
      }
    },
    {
      timeout: 15000,
      maximumAge: 300000  // Accept a 5-minute cached position
    }
  );
}

// ----- Pebble events -----
Pebble.addEventListener('ready', function() {
  console.log('Sweat Factor:PebbleKit JS ready');
  getWeather();
});

Pebble.addEventListener('appmessage', function(e) {
  if (e.payload && e.payload['REQUEST_WEATHER']) {
    console.log('Sweat Factor:watch requested weather refresh');
    getWeather();
  }
});
