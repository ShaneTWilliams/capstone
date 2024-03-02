"use client";

import React, { useState, useEffect } from 'react'
import Navbar from './navbar'
import MainContent from './main-content';
import StatusDialog from "./dialogs/status-dialog"
import SettingsDialog from "./dialogs/settings-dialog"
import MotorPage from "./pages/motor-page"
import AllValuesPage from "./pages/all-values-page"
import yaml from "../../../values/values.json";

const {GetValueRequest} = require('../../proto/ground_pb.js');
const {GroundStationClient} = require('../../proto/ground_grpc_web_pb.js');

function getValueStatus(valueConfig, value) {
  if (valueConfig.conditions == null) {
    return "Nominal";
  }

  for (let level of ["Critical", "Warning"]) {
    if (valueConfig.conditions[level.toLowerCase()] == null) {
      continue;
    }
    for (let condition of valueConfig.conditions[level.toLowerCase()]) {
      if ((condition.over != null && value > condition.over)
        || (condition.under != null && value < condition.under)
        || (condition.equal != null && value == condition.equal)) {
        return level;
      }
    }
  }
  return "Nominal";
}

function App() {
  const [settingsDialogOpen, setSettingsDialogOpen] = useState(false)
  const [statusDialogOpen, setStatusDialogOpen] = useState(false)
  const [settings, setSettings] = useState({ theme: 'dark' })
  const [activeMenuItem, setActiveMenuItem] = useState("All Values");
  const [currentValues, setCurrentValues] = useState({});
  const [overallStatus, setOverallStatus] = useState("Nominal");
  const [counter, setCounter] = useState(0);

  var client = new GroundStationClient('http://localhost:50052');

  const updateSetting = (setting, value) => {
    setSettings({ [setting]: value });
  }

  useEffect(() => {
    const interval = setInterval(() => {
      let index = 0;
      for (const [value, valueConfig] of Object.entries(yaml.values)) {
        var request = new GetValueRequest();
        request.setTag(index);
        index++;

        client.getValue(request, {}, (err, response) => {
          let newValue = null;
          if (valueConfig.type.base == "bool") {
            newValue = response.getB();
          } else if (valueConfig.type.base == "decimal") {
            newValue = response.getF32();
          } else if (valueConfig.type.base == "int" || valueConfig.type.base == "enum") {
            newValue = response.getU32();
          }
          const newStatus = getValueStatus(yaml.values[value], newValue);

          setCurrentValues((prevState) => {
            let newHistory1s;
            if (prevState[value] == null) {
              newHistory1s = [newValue];
            } else {
              newHistory1s = prevState[value]["history1s"].concat(newValue);
              if (newHistory1s.length > 10) {
                newHistory1s = newHistory1s.slice(1);
              }
            }
            return {
              ...prevState,
              [value]: {
                "history1s": newHistory1s,
                "value": newValue,
                "status": newStatus
              }
            }
          });
        });
      }
      setCounter(counter + 1);
    }, 100);

    return () => clearInterval(interval);
  }, []);

  useEffect(() => {
    let newOverallStatus = "Nominal";
    for (const value of Object.keys(currentValues)) {
      const status = currentValues[value].status;
      if (status == "Critical") {
        newOverallStatus = "Critical";
        break;
      } else if (status != "Critical" && status == "Warning") {
        newOverallStatus = "Warning"
      }
    }
    setOverallStatus(newOverallStatus);
  }, [currentValues])

  let theme = "";
  if (settings.theme == "dark" || (settings.theme == "system" && window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches)) {
    theme = "dark";
  }

  const menuItems = {
    "All Values": {
      content: <AllValuesPage currentValues={currentValues} />,
      showStatus: false
    },
    "Motors": {
      content: <MotorPage currentValues={currentValues} />,
      showStatus: true
    },
    "Power & Battery": {
      content: <MotorPage currentValues={currentValues} />,
      showStatus: true
    },
    "Digital Radio": {
      content: <MotorPage currentValues={currentValues} />,
      showStatus: true
    },
    "Location": {
      content: <MotorPage currentValues={currentValues} />,
      showStatus: true
    },
    "Temperature": {
      content: <MotorPage currentValues={currentValues} />,
      showStatus: true
    }
  }

  return (
    <div
      className={`flex flex-row h-screen items-stretch select-none cursor-default overflow-auto fixed w-screen ${theme}`}
    >
      <div className="flex flex-col w-60 border-r border-neutral-200 bg-neutral-100 dark:bg-zinc-800 dark:border-zinc-700">
        <div className="flex-none h-7"></div>
        <Navbar
          menuItems={menuItems}
          activeMenuItem={activeMenuItem}
          onSelectContent={setActiveMenuItem}
          onSettingsDialog={() => { setSettingsDialogOpen(true) }}
          onStatusDialog={() => { setStatusDialogOpen(true) }}
          overallStatus={overallStatus}
        />
      </div>
      <div className="flex flex-col grow bg-neutral-50 h-screen w-0 dark:bg-zinc-900">
        <div className="flex-none h-7"></div>
        <div className="grow h-0">
          <MainContent
            title={activeMenuItem}
            content={menuItems[activeMenuItem]["content"]}
          />
        </div>
      </div>
      {
        settingsDialogOpen && (
          <SettingsDialog
            onExit={() => { setSettingsDialogOpen(false) }}
            settings={settings}
            setSetting={updateSetting}
          />
        )
      }
      {
        statusDialogOpen && (
          <StatusDialog
            onExit={() => { setStatusDialogOpen(false) }}
          />
        )
      }
      <div data-tauri-drag-region className="fixed w-screen h-8"></div>
    </div >
  )
}

export default App;
