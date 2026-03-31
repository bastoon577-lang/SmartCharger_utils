const char* cssSteelSheet PROGMEM = R"rawliteral(
body{
margin: 0;
padding: 0;
font-family: Arial, sans-serif;
text-align: center;
background: var(--bg-body);
color: var(--text-main);
}
.hamburger{
position: fixed;
top: 10px;
left: 10px;
cursor: pointer;
width: 30px;
height: 30px;
display: flex;
flex-direction: column;
justify-content: space-around;
z-index: 1100;
}
.hamburger div{
width: 30px;
height: 4px;
border-radius: 2px;
transition: 0.3s;
background-color: var(--text-main);
}
.nav-menu{
position: fixed;
top: 0;
left: -100%;
width: 70%;
max-width: 300px;
height: 100vh;
padding-top: 50px;
transition: 0.3s;
z-index: 1000;
display: flex;
flex-direction: column;
align-items: flex-start;
padding-left: 20px;
background-color: var(--nav-bg);
color: var(--nav-text);
}
.nav-menu.open{
left: 0;
}
.nav-menu a{
text-decoration: none;
font-size: 1.2rem;
margin: 15px 0;
cursor: pointer;
color: var(--nav-text);
}
.nav-menu a:hover{
color: var(--nav-hover);
}
.button{
display: block;
width: 90%;
max-width: 300px;
padding: 15px;
margin: 10px auto;
font-size: 1.2rem;
font-weight: bold;
border: none;
border-radius: 8px;
text-decoration: none;
cursor: pointer;
transition: background-color 0.3s;
background-color: var(--primary);
color: #fff;
}
.button:hover{
background-color: var(--primary-hover);
}
.button.secondary{
background-color: var(--secondary);
}
.button.secondary:hover{
background-color: var(--secondary-hover);
}
.button.clicked{
background: var(--clicked);
color: #ffffff;
}
.button.clicked:hover{
background: var(--clicked-hover);
}
.button.warning{
background-color: var(--warning);
}
.button.warning:hover{
background-color: var(--warning-hover);
}
.button.small {
display: inline-flex;
width: 60px;
max-width: none;
margin: 0;
justify-content: center;
align-items: center;
padding: 15px 0;
}
.fade-text{
animation: fade 1s infinite alternate;
}
@keyframes fade{
from { opacity: 1; }
to { opacity: 0.5; }
}
input{
width: 80%;
padding: 10px;
margin: 10px 0;
border-radius: 5px;
border: none;
background-color: var(--input-bg);
color: var(--input-text);
}
table{
border: 1px solid #e2e8f0;
width: 100%;
border-collapse: collapse;
margin-top: 20px;
}
th, td{
padding: 12px;
text-align: left;
width: 50%;
}
th{
background-color: var(--table-th-bg);
color: var(--table-th-text);
}
td{
background-color: var(--table-td-bg);
word-wrap: break-word;
}
td:hover{
background-color: var(--table-td-hover);
}
table, th, td{
border: 1px solid var(--table-th-td);
}
.battery{
position: relative;
width: 200px;
height: 80px;
border-radius: 15px;
margin: 50px auto;
display: flex;
align-items: center;
justify-content: space-around;
background-color: var(--battery-bg);
}
.battery::after{
content: '';
position: absolute;
top: 50%;
right: -10px;
transform: translateY(-50%);
width: 20px;
height: 30px;
border-radius: 10px;
background-color: var(--battery-tip);
}
.battery-level{
display: flex;
gap: 5px;
}
.battery-level{
display:flex;
gap:5px;
height:100%;
align-items:center;
}
.battery-level div{
width: 35px;
height: 60px;
border-radius: 5px;
}
.battery.red .battery-level div{
animation: fade 1s infinite alternate;
background-color: #c02d2a;
}
.battery.green .battery-level div{
animation: fade 1s infinite alternate;
background-color: #10b981;
}
.battery.orange .battery-level div{
animation: fade 1s infinite alternate;
background-color: #e36145;
}
.alert{
position: relative;
padding: 15px;
margin: 20px;
border-radius: 5px;
justify-content: space-between;
align-items: center;
background-color: var(--alert-bg);
color: var(--alert-text);
border: 1px solid var(--alert-border);
}
.container{
display: none;
}
.container.active{
display: block;
}
.number-control {
display: flex;
justify-content: center;
align-items: center;
gap: 25px;
}
:root{
--bg-body: #f8fafc;
--text-main: #1f2937;
--primary: #2563eb;
--primary-hover: #1d4ed8;
--secondary: #64748b;
--secondary-hover: #475569;
--clicked: #16a34a;
--clicked-hover: #15803d;
--warning: #dc2626;
--warning-hover: #b91c1c;
--nav-bg: #2563eb;
--nav-text: #ffffff;
--nav-hover: #dbeafe;
--input-bg: #ffffff;
--input-text: #1f2937;
--table-border: #e2e8f0;
--table-th-bg: #2563eb;
--table-th-text: #ffffff;
--table-td-bg: #ffffff;
--table-td-hover: #f3f4f6;
--table-th-td:#e2e8f0;
--battery-bg: #d1d5db;
--battery-tip: #1f2937;
--alert-bg: #f8d7da;
--alert-text: #721c24;
--alert-border: #f5c6cb;
}
body.theme-dark{
--bg-body: #1a202c;
--text-main: #f8fafc;
--primary: #2b6cb0;
--primary-hover: #2c5282;
--secondary: #4a5568;
--secondary-hover: #2d3748;
--clicked: #22c55e;
--clicked-hover: #16a34a;
--warning: #c53030;
--warning-hover: #9b2c2c;
--nav-bg: #2b6cb0;
--nav-text: #f8fafc;
--nav-hover: #90cdf4;
--input-bg: #2d3748;
--input-text: #ffffff;
--table-border: #4a5568;
--table-th-bg: #2b6cb0;
--table-th-text: #f8fafc;
--table-td-bg: #3b4b5c;
--table-td-hover: #4a5568;
--table-th-td:#4a5568;
--battery-bg: #2d3748;
--battery-tip: #4a5568;
--alert-bg: #3b0d0d;
--alert-text: #f8d7da;
--alert-border: #a94442;
}
)rawliteral";
