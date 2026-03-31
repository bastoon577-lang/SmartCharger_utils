const char* htmlHeader PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>SmartCharger</title>
)rawliteral";

const char* scriptsCommon PROGMEM = R"rawliteral(  
  function sendPostRequest(url,data,callback) {
    var xhr = new XMLHttpRequest();
    xhr.open('POST', url, true);
    xhr.setRequestHeader('Content-Type','application/x-www-form-urlencoded');
    xhr.send(data);
    xhr.onload = function() {
      if (xhr.status === 200) {
        callback(200);
      } else {
        return;
      }
    };
  }
  
  function toggleTheme() {
    document.body.classList.toggle('theme-dark');
  }

  function next(step) {
    activerContainer(step);
  }

  function activerContainer(id) {
    var containers = document.querySelectorAll('.container');
    containers.forEach(function(container) {
      container.classList.remove('active');
    });
    document.getElementById('container' + id).classList.add('active');
  }

  function handleRequest(request,fun) {
    sendPostRequest('/'+request,'data='+request,function(status) {
      if (status === 200) {
        fun()
      }
    });
  }

  function handleComplexRequest(request,data,fun) {
    sendPostRequest('/'+request,data,function(status) {
      if (status === 200) {
        fun()
      }
    });
  }
)rawliteral";
