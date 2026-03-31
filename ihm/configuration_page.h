const char* htmlPageConfig PROGMEM = R"rawliteral(
  <div id="container1" class="container active">
    <h1>Bienvenue</h1>
    <p>Sur votre nouvelle station de recharge SmartCharger. Nous allons configurer pas à pas votre équipement.</p>
    <button class="button" onclick="next(2)">Suivant</button>
  </div>

  <div id="container2" class="container">
    <h1>Réseau électrique</h1>
    <p>Quel est votre réseau électrique ?</p>
    <button class="button" onclick="handleRequest('vol0',function(){next(4);})">230V Monophasé</button>
    <button class="button" onclick="handleRequest('vol1',function(){next(4);})">400V Triphasé</button>
  </div>

  <div id="container4" class="container">
    <h1>Configuration Wi-Fi</h1>
    <p>Souhaitez-vous connecter votre SmartCharger sur un point d'accès Wi-Fi de votre domicile ?</p>
    <button class="button" onclick="handleRequest('wif1',function(){next(5);})">Oui</button>
    <button class="button secondary" onclick="handleRequest('wif0',function(){next(15);})">Non</button>
  </div>

  <div id="container5" class="container">
    <h1>Configuration Wi-Fi</h1>
    <div id="wifiList">
    </div>
    <button class="button warning" onclick="next(6)">Saisie manuelle</button>
  </div>

  <div id="container6" class="container">
    <h1>Configuration Wi-Fi</h1>
    <input type="text" id="ssid1" placeholder="Entrez le SSID">
    <button class="button" id="ssid1-submit">Soumettre</button>
  </div>

  <div id="container7" class="container">
    <h1>Configuration Wi-Fi</h1>
    <input type="password" id="mdp1" placeholder="Entrez le mot de passe">
    <input type="password" id="mdp11" placeholder="Confirmez le mot de passe">
    <button class="button" id="mdp1-submit">Soumettre</button>
  </div>

  <div id="container8" class="container">
    <h1>Configuration Wi-Fi</h1>
    <input type="text" id="hot1" placeholder="Entrez l'Hostname">
    <button class="button" id="hot1-submit">Soumettre</button>
  </div>

  <div id="container9" class="container">
    <h1>Configuration Wi-Fi</h1>
    <button class="button secondary" onclick="toggleAdvancedConfig()">Configurations avancées</button>
    <div id="advancedConfig" style="display: none;">
      <input type="text" id="ip" placeholder="Adresse IP de l'équipement">
      <input type="text" id="mask" placeholder="Masque de sous-réseau">
      <input type="text" id="gateway" placeholder="Passerelle">
    </div>
    <button class="button" id="advanced-submit">Soumettre</button>
  </div>

  <div id="container15" class="container">
    <h1>Configuration Wi-Fi</h1>
    <p>Votre SmartCharger restera accessible en Wi-Fi sur ces identifiants. Souhaitez-vous personnaliser les identifiants Wi-Fi (SSID/Clé de sécurité réseau)?</p>
    <p class="alert">Il est fortement recommandé de modifier ces paramètres !</p>
    <button class="button" onclick="next(16)">Oui</button>
    <button class="button secondary" onclick="next(20)">Non</button>
  </div>

  <div id="container16" class="container">
    <h1>Configuration Wi-Fi</h1>
    <input type="text" id="ssid2" placeholder="Entrez le SSID">
    <button class="button" id="ssid2-submit">Soumettre</button>
  </div>

  <div id="container17" class="container">
    <h1>Configuration Wi-Fi</h1>
    <input type="password" id="mdp2" placeholder="Entrez le mot de passe">
    <input type="password" id="mdp22" placeholder="Confirmez le mot de passe">
    <button class="button" id="mdp2-submit">Soumettre</button>
  </div>

  <div id="container20" class="container">
    <h1>Module TIC</h1>
    <p>Souhaitez-vous configurer un Module TIC ?</p>
    <button class="button" onclick="handleRequest('tic1',function(){next(21);})">Oui</button>
    <button class="button secondary" onclick="handleRequest('tic0',function(){next(30);})">Non</button>
  </div>

  <div id="container21" class="container">
    <h1>Module TIC</h1>
    <input type="text" id="host1" placeholder="Hostname ou adresse IP du Module TIC">
    <input type="text" id="port1" placeholder="Port WS du Module TIC">
    <button class="button" id="tic-submit">Soumettre</button>
  </div>

  <div id="container30" class="container">
    <h1>Félicitation</h1>
    <p>Vous avez terminé la configuration de votre équipement SmartCharger. Votre équipement doit redémarrer...</p>
    <button class="button" onclick="handleRequest('end',function(){next(31);})">Redémarrer</button>
  </div>

  <div id="container31" class="container">
    <h1>Redémarrage</h1>
    <p>En cours...</p>
  </div>
)rawliteral";

const char* scriptsPageConfig PROGMEM = R"rawliteral(
    document.querySelectorAll('button[data-action]').forEach(function(button) {
      button.onclick = function() {
        var action = button.getAttribute('data-action');
        sendPostRequest('/' + action, 'data=' + action);
      };
    });

    function validateField(id,value) {
      if (id.startsWith('ssid')) {
        return validateSSID(value);
      }
      if (id.startsWith('hot1')) {
        return validateHostname(value);
      }
      if (id.startsWith('mdp')) {
        return validatePassword(id, value);
      }
      return true;
    }

    function validateSSID(value) {
      if (value.length < 5 || value.length > 33) {
        alert("Le SSID doit contenir entre 5 et 33 caractères.");
        return false;
      }
      return true;
    }

    function validateHostname(value) {
      if (value.length === 0) {
        return true;
      }
      if (value.length < 5 || value.length > 65) {
        alert("Le nom d'hôte doit contenir entre 5 et 65 caractères.");
        return false;
      }
      return true;
    }

    function validatePassword(id, value) {
      const confirmId = (id === 'mdp1') ? 'mdp11' : (id === 'mdp2') ? 'mdp22' : null;
      if (confirmId) {
        const confirmValue = document.getElementById(confirmId).value;
        if (value !== confirmValue) {
          alert("Les mots de passe ne correspondent pas. Veuillez réessayer.");
          return false;
        }
      }
      if (value.length < 5 || value.length > 65) {
        alert("La clé de sécurité doit contenir entre 5 et 65 caractères.");
        return false;
      }
      return true;
    }

    function handleRequestAndProceed(id, value, nextContainerId) {
      if (id.startsWith('ssid')) {
        handleComplexRequest("ssi1","ssid="+encodeURIComponent(value),function(){
          next(nextContainerId);
        });
      }
      if (id.startsWith('hot1')) {
        handleComplexRequest("hot1","hot1="+encodeURIComponent(value),function(){
          next(nextContainerId);
        });
      }
      if (id.startsWith('mdp')) {
        handleComplexRequest("pas1","pass="+encodeURIComponent(value),function(){
          next(nextContainerId);
        });
      }
    }

    function validateAndProceed(id, nextContainerId) {
      const value = document.getElementById(id).value;
      if (!validateField(id,value)) {
        return;
      }
      handleRequestAndProceed(id,value,nextContainerId);
    }

    document.getElementById('ssid1-submit').addEventListener('click',function(){
      validateAndProceed('ssid1',7);
    });

    document.getElementById('mdp1-submit').addEventListener('click',function(){
      validateAndProceed('mdp1',8);
    });

    document.getElementById('hot1-submit').addEventListener('click',function(){
      validateAndProceed('hot1',9);
    });

    document.getElementById('ssid2-submit').addEventListener('click',function(){
      validateAndProceed('ssid2',17);
    });

    document.getElementById('mdp2-submit').addEventListener('click',function(){
      validateAndProceed('mdp2',20);
    });

    function formatIp(ip) {
      var parts = ip.split('.');
      for (var i = 0; i < parts.length; i++)
        parts[i] = parts[i].padStart(3, '0');
      return parts.join('.');
    }

    function isValidIp(ip) {
      const ipPattern = /^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/;
      return ipPattern.test(ip);
    }

    function sendAdvConfig() {
      const ip = document.getElementById('ip').value;
      const mask = document.getElementById('mask').value;
      const gateway = document.getElementById('gateway').value;
      if (!ip && !mask && !gateway) {
        next(20);
        return;
      }
      if (!isValidIp(ip)) {
        alert("L'adresse IP est invalide. Veuillez entrer une adresse IP valide.");
        return;
      }
      if (!isValidIp(mask)) {
        alert("Le masque de sous-réseau est invalide. Veuillez entrer un masque valide.");
        return;
      }
      if (!isValidIp(gateway)) {
        alert("La passerelle est invalide. Veuillez entrer une adresse de passerelle valide.");
        return;
      }
      const formattedIp = formatIp(ip);
      const formattedMask = formatIp(mask);
      const formattedGateway = formatIp(gateway);
      const data = `Cha=1&ip=${encodeURIComponent(formattedIp)}&mask=${encodeURIComponent(formattedMask)}&gateway=${encodeURIComponent(formattedGateway)}`;
      handleComplexRequest("adv0",data,function(){next(20);});
    }

    function sendTicConfig() {
      const host = document.getElementById('host1').value;
      const port = document.getElementById('port1').value;
      if(host.length < 5 || host.length > 66) {
        alert("Le nom d'hôte doit contenir entre 5 et 65 caractères ou l'adresse IP doit être de la forme 192.168.1.2.");
        return;
      }
      if(!port || isNaN(port) || port < 1 || port > 65535) {
        alert("Le port est invalide. Veuillez entrer un numéro entre 1 et 65535.");
        return;
      }
      const data = `Tic=1&host=${encodeURIComponent(host)}&port=${encodeURIComponent(port)}`;
      handleComplexRequest("adv0",data,function(){next(30);});
    }

    document.getElementById('advanced-submit').addEventListener('click',sendAdvConfig);
    document.getElementById('tic-submit').addEventListener('click',sendTicConfig);

    function addWifiSpot(ssid) {
      var wifiList = document.getElementById("wifiList");
      var button = document.createElement("button");
      button.className = "button";
      button.textContent = ssid;
      button.onclick = function () {
        sendSSIDFromButton(ssid);
      };
      wifiList.appendChild(button);
    }

    function sendSSIDFromButton(ssid) {
      sendPostRequest('/ssi1', 'ssid=' + encodeURIComponent(ssid),function(){next(7);});
    }

    function toggleAdvancedConfig() {
      var configDiv = document.getElementById('advancedConfig');
      configDiv.style.display = configDiv.style.display === 'none' ? 'block' : 'none';
    }
)rawliteral";
