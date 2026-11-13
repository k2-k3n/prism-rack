// Prism Rack: what the controllers show and the CC each control sends (channel 1).
// src/app.cpp binds the same numbers in kBind.
export const profile = {
  name: 'Prism Rack',
  accent: '#1fb89e',
  pages: [
    {key: 'P', label: 'PLAY', title: 'Play the rack', color: '#28E2C2', controls: [
      {id: 'fire', cc: 30, kind: 'trigger', label: 'FIRE', key: 'A'},
      {id: 'dub', cc: 31, kind: 'trigger', label: 'DUB', key: 'S'},
      {id: 'kill', cc: 32, kind: 'trigger', label: 'KILL', key: 'D'},
      {id: 'dryHold', cc: 33, kind: 'trigger', label: 'DRY', key: 'F'},
      {id: 'prism', kind: 'xy', label: 'Blend × Drive', x: 'blend', y: 'drive'},
    ]},
    {key: 'S', label: 'SCENE', title: 'Scene', color: '#8C7CF0', controls: [
      {id: 'scene', cc: 20, kind: 'choice', label: 'Scene', options: ['GLASS', 'EMBER', 'HALO'], def: 2},
      {id: 'blend', cc: 21, kind: 'range', label: 'Blend', min: 0, max: 100, unit: '%', def: 35},
      {id: 'bypass', cc: 23, kind: 'toggle', label: 'Bypass', def: 0},
      {id: 'input', cc: 22, kind: 'choice', label: 'Input', options: ['GTR', 'USB'], def: 0},
    ]},
    {key: 'D', label: 'DRIVE', title: 'Cell A · drive', color: '#FF6B5B', controls: [
      {id: 'drive', cc: 24, kind: 'range', label: 'Drive', min: 0, max: 100, unit: '%', def: 25},
      {id: 'tone', cc: 25, kind: 'range', label: 'Tone', min: 0, max: 100, unit: '%', def: 60},
      {id: 'level', cc: 26, kind: 'range', label: 'Level', min: 0, max: 100, unit: '%', def: 80},
      {id: 'driveOn', cc: 43, kind: 'toggle', label: 'Cell A', def: 1},
    ]},
    {key: 'E', label: 'ECHO', title: 'Cell B · echo', color: '#FFA54D', controls: [
      {id: 'time', cc: 27, kind: 'range', label: 'Time', min: 40, max: 500, unit: 'ms', def: 180},
      {id: 'feedback', cc: 28, kind: 'range', label: 'Feedback', min: 0, max: 70, unit: '%', def: 25},
      {id: 'echoOn', cc: 44, kind: 'toggle', label: 'Cell B', def: 1},
    ]},
    {key: 'R', label: 'RACK', title: 'The rack', color: '#6F909E', controls: [
      {id: 'trim', cc: 40, kind: 'range', label: 'Input trim', min: 0, max: 100, unit: '%', def: 80},
      {id: 'gate', cc: 29, kind: 'range', label: 'Gate threshold', min: 0, max: 100, unit: '%', def: 0},
      {id: 'gateOn', cc: 41, kind: 'toggle', label: 'Gate', def: 1},
      {id: 'compOn', cc: 42, kind: 'toggle', label: 'Comp', def: 1},
      {id: 'cpu', cc: 45, kind: 'readout', label: 'CPU', min: 0, max: 100, def: 0, format: v => `${v}%`},
      {id: 'save', cc: 34, kind: 'trigger', label: 'SAVE', key: 'W'},
    ]},
  ],
};
