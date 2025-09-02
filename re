import React, { useEffect, useRef, useState } from "react";
import L from "leaflet";
import "leaflet/dist/leaflet.css";

// Utility for geographic calculations
function offsetPoint(latlng, bearingDeg, distanceMeters) {
const R = 6378137;
const dByR = distanceMeters / R;
const bearing = (bearingDeg * Math.PI) / 180;
const lat1 = (latlng.lat * Math.PI) / 180;
const lon1 = (latlng.lng * Math.PI) / 180;

const lat2 = Math.asin(
Math.sin(lat1) * Math.cos(dByR) + Math.cos(lat1) * Math.sin(dByR) * Math.cos(bearing)
);
const lon2 =
lon1 +
Math.atan2(
Math.sin(bearing) * Math.sin(dByR) * Math.cos(lat1),
Math.cos(dByR) - Math.sin(lat1) * Math.sin(lat2)
);
return L.latLng((lat2 * 180) / Math.PI, (lon2 * 180) / Math.PI);
}

const defaultView = { lat: -31.95, lng: 115.86, zoom: 4 };

function DisasterSimulator({ mapHeight = 500 }) {
const mapRef = useRef(null);
const [map, setMap] = useState(null);
const [disasterType, setDisasterType] = useState("earthquake");
const [pinnedLocation, setPinnedLocation] = useState(null);
const [damageVisuals, setDamageVisuals] = useState([]);
const [impactSummary, setImpactSummary] = useState("");
const [simulateEnabled, setSimulateEnabled] = useState(false);
const [magnitude, setMagnitude] = useState(7.0);
const [meteorSize, setMeteorSize] = useState(100);
const [fireIntensity, setFireIntensity] = useState(5);
const [windSpeed, setWindSpeed] = useState(20);
const [windDir, setWindDir] = useState(45);
const [vei, setVei] = useState(4);
const [ashWind, setAshWind] = useState(90);
const [canTsunami, setCanTsunami] = useState(true);
const markerRef = useRef(null);

useEffect(() => {
if (!mapRef.current || map) return;
const leafletMap = L.map(mapRef.current).setView(
[defaultView.lat, defaultView.lng],
defaultView.zoom
);
L.tileLayer(
"https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png",
{
maxZoom: 19,
attribution:
'© OpenStreetMap © CARTO',
}
).addTo(leafletMap);
setMap(leafletMap);
}, [mapRef, map]);

useEffect(() => {
if (!map) return;
function onMapClick(e) {
setPinnedLocation(e.latlng);
setSimulateEnabled(true);
if (!markerRef.current) {
markerRef.current = L.marker(e.latlng).addTo(map);
} else {
markerRef.current.setLatLng(e.latlng);
}
checkWaterLocation(e.latlng.lat, e.latlng.lng);
}
map.on("click", onMapClick);
return () => map.off("click", onMapClick);
}, [map, disasterType]);

async function checkWaterLocation(lat, lng) {
const url = https://nominatim.openstreetmap.org/reverse?format=jsonv2&lat=${lat}&lon=${lng};
try {
const resp = await fetch(url, { headers: { "Accept-Language": "en" } });
const data = await resp.json();
const onLand = !!(data && data.address && data.address.country_code);
setCanTsunami(!onLand);
if (onLand && disasterType === "tsunami") setDisasterType("earthquake");
} catch (e) {
setCanTsunami(false);
}
}

function calculateEarthquakeDamage() {
return {
meta: { magnitude },
zones: [
{ radius: magnitude * 8000, color: "#fef08a", opacity: 0.3, label: "Light" },
{ radius: magnitude * 2500, color: "#fb923c", opacity: 0.4, label: "Moderate" },
{ radius: magnitude * 400, color: "#ef4444", opacity: 0.5, label: "Severe" },
],
};
}
function calculateTsunamiDamage() {
return {
meta: { magnitude },
zones: [
{ radius: magnitude * 30000, color: "#22d3ee", opacity: 0.2, label: "Wave 1" },
{ radius: magnitude * 15000, color: "#22d3ee", opacity: 0.2, label: "Wave 2" },
{ radius: magnitude * 5000, color: "#22d3ee", opacity: 0.2, label: "Wave 3" },
],
};
}
function calculateMeteorDamage() {
return {
meta: { size_m: meteorSize },
zones: [
{ radius: meteorSize * 150, color: "#fb923c", opacity: 0.4, label: "Air Blast" },
{ radius: meteorSize * 25, color: "#ef4444", opacity: 0.6, label: "Crater" },
],
};
}
function calculateFireSpread() {
const base = 500 + fireIntensity * 400;
const windFactor = 1 + windSpeed / 60;
const steps = 5;
const stepDist = (200 + fireIntensity * 120) * windFactor;
const zones = [];
for (let i = 1; i <= steps; i++) {
const r = base * (0.6 + i * 0.2) * windFactor;
const offset = i * stepDist;
const center = offsetPoint(pinnedLocation, windDir, offset);
zones.push({
center,
radius: r,
color: "#f97316",
opacity: 0.28,
label: i === steps ? "Head Fire" : "Flank",
});
}
zones.unshift({
center: pinnedLocation,
radius: base * 0.6,
color: "#ef4444",
opacity: 0.35,
label: "Origin",
});
return { meta: { fireIntensity, windSpeed, windDir }, zones, kind: "offset" };
}
function calculateVolcanoImpact() {
const baseAshKm = 5 + vei * 15;
const farAshKm = baseAshKm * (1.5 + vei * 0.2);
const zones = [
{ radius: baseAshKm * 0.5 * 1000, color: "#9ca3af", opacity: 0.45, label: "Proximal" },
{ radius: baseAshKm * 1000, color: "#6b7280", opacity: 0.35, label: "Ashfall" },
];
const steps = 4;
for (let i = 1; i <= steps; i++) {
const frac = i / steps;
const dist = (baseAshKm * 400 + farAshKm * 0.6 * frac) * 1000;
const r = farAshKm * (0.7 + frac * 0.8) * 1000;
const center = offsetPoint(pinnedLocation, ashWind, dist);
zones.push({
center,
radius: r,
color: "#4b5563",
opacity: 0.18,
label: "Ash Cloud",
});
}
return { meta: { vei, ashWind }, zones, kind: "offset" };
}

function drawDamageOnMap(data) {
if (!map || !pinnedLocation || !data || !data.zones) return;
const overlays = [];
const useOffsets = data.kind === "offset";
data.zones.forEach((zone) => {
const center = useOffsets && zone.center ? zone.center : pinnedLocation;
const circle = L.circle(center, {
radius: zone.radius,
color: zone.color,
fillColor: zone.color,
fillOpacity: zone.opacity,
weight: 1,
}).addTo(map);
circle.bindTooltip(${zone.label} Zone);
overlays.push(circle);
});
setDamageVisuals(overlays);
if (overlays.length > 0) {
const group = L.featureGroup(overlays);
map.fitBounds(group.getBounds(), { padding: [20, 20] });
}
}

function clearVisuals() {
damageVisuals.forEach((layer) => map.removeLayer(layer));
setDamageVisuals([]);
}

function runSimulation() {
if (!pinnedLocation) {
alert("Pick a target first.");
return;
}
clearVisuals();
let damageData;
switch (disasterType) {
case "earthquake":
damageData = calculateEarthquakeDamage();
break;
case "tsunami":
damageData = calculateTsunamiDamage();
break;
case "meteor":
damageData = calculateMeteorDamage();
break;
case "fire":
damageData = calculateFireSpread();
break;
case "volcano":
damageData = calculateVolcanoImpact();
break;
default:
damageData = null;
}
drawDamageOnMap(damageData);
setImpactSummary(generateImpactSummary(disasterType, damageData));
}

function generateImpactSummary(type, data) {
const { meta = {} } = data || {};
const loc = pinnedLocation
? ${pinnedLocation.lat.toFixed(2)}, ${pinnedLocation.lng.toFixed(2)}
: "unknown";
const dirTxt = (d) => ${d}°;

switch (type) {
  case "earthquake": {
    const scale =
      meta.magnitude >= 8.5
        ? "catastrophic"
        : meta.magnitude >= 7.5
        ? "severe"
        : "moderate";
    return `Quake M${meta.magnitude?.toFixed(1)} near ${loc}. Expected ${scale} shaking near epicenter with diminishing damage out to ~${Math.round(
      meta.magnitude * 8
    )} km.`;
  }
  case "tsunami": {
    const wave =
      meta.magnitude >= 8.5
        ? "very large"
        : meta.magnitude >= 7.5
        ? "large"
        : "moderate";
    return `Tsunami triggered by M${meta.magnitude?.toFixed(
      1
    )} offshore event at ${loc}. Anticipate ${wave} waves; first arrivals could reach coastlines within hours depending on bathymetry.`;
  }
  case "meteor": {
    const classif =
      meta.size_m >= 500
        ? "city-ending"
        : meta.size_m >= 100
        ? "city-scale"
        : "local";
    return `Meteor ~${Math.round(
      meta.size_m
    )} m at ${loc}. Air-blast and crater indicate ${classif} impact. Glass breakage tens of km; crater diameter ~${Math.round(
      meta.size_m * 0.02
    )} km (toy model).`;
  }
  case "fire": {
    const rate =
      windSpeed >= 60
        ? "explosive spread"
        : windSpeed >= 30
        ? "fast spread"
        : "steady spread";
    return `Wildfire ignition at ${loc}. Intensity ${fireIntensity}/10{ pkgs }: {
deps = [ pkgs.nodejs pkgs.yarn ];
}run = "npm run dev"literati-disaster-map-react/ ├─ package.json ├─ vite.config.js ├─ .gitignore ├─ README.md ├─ public/ │ └─ favicon.ico ├─ .replit ├─ replit.nix └─ src/ ├─ main.jsx ├─ App.jsx ├─ components/ │ └─ DisasterMap.jsx └─ index.css

.replit

run = "npm run dev"

replit.nix

{ pkgs }: {
deps = [ pkgs.nodejs pkgs.yarn ];
}

This setup allows your Literati Disaster Map React project to run directly on Replit with the Run button, using Node.js and Vite. The .replit file tells Replit to start the dev server, and replit.nix ensures the correct Node environment is available.