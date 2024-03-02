import colors from "tailwindcss/colors";
import yaml from "../../../../values/values.json";

import { useState, useYaml } from 'react'
import {
  Chart as ChartJS,
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend,
} from 'chart.js';
import { Line } from 'react-chartjs-2';

ChartJS.register(
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend
);

const labels = [...Array(10).keys()];;

function ValueEntry({ valueConfig, value, isActive, onClick }) {
  let displayValue = value.value;
  if (valueConfig.type.base == "bool") {
    displayValue = (value.value ? "On" : "Off")
  } else if (valueConfig.type.base == "decimal") {
    displayValue = Number((value.value)).toFixed(2)
  }

  let valueStatusColor = "bg-green-500";
  if (value.status == "Warning") {
    valueStatusColor = "bg-yellow-500";
  } else if (value.status == "Critical") {
    valueStatusColor = "bg-red-500";
  }

  const data = {
    labels,
    datasets: [
      {
        data: value.history1s,
        borderColor: colors.red[400],
        textColor: colors.red[400],
      },
    ],
  };

  let min, max;
  if (valueConfig.type.base == "bool") {
    min = 0;
    max = 1;
  } else if (valueConfig.type.base == "decimal" || valueConfig.type.base == "int") {
    min = valueConfig.type.min;
    max = valueConfig.type.max;
  } else if (valueConfig.type.base == "enum") {
    min = 0;
    max = yaml.enums[valueConfig.type.name].length;
  }

  const options = {
    responsive: true,
    plugins: {
      legend: {
        display: false,
      },
    },
    scales: {
      y: {
        min: min,
        max: max,
      }
    }
  };

  return (
    <div className="flex grow flex-col justify-start cursor-default border-b border-neutral-200 last:border-0 p-2 dark:bg-zinc-900 dark:border-zinc-700">
      <div onClick={onClick} className="flex flex-row justify-start hover:cursor-pointer dark:bg-zinc-900 pl-2 pr-6">
        <div className="w-64">
          <p className="text-neutral-800 dark:text-zinc-200">
            {valueConfig.display}
          </p>
        </div>
        {!isActive && <div className="w-36">
          <p className="text-neutral-400 dark:text-zinc-400">
            {displayValue} {valueConfig.unit}
          </p>
        </div>}
        <div className="flex-grow flex flex-row justify-end">
          <div className={`rounded-sm flex justify-center px-2 rounded-md w-20 ${valueStatusColor}`}>
            <p className="text-white">
              {value.status}
            </p>
          </div>
        </div>
      </div>
      { isActive && <div className="flex-row flex items-center my-3">
          <div className="flex flex-row items-center justify-center rounded-md h-48 w-96 ml-4 text-neutral-400 p-3">
            <Line options={options} data={data} />
          </div>
          <p className="text-5xl ml-20 text-neutral-800 dark:text-zinc-200">{displayValue}</p>
          <p className="text-5xl ml-1 text-neutral-400 font-light">{valueConfig.unit}</p>
        </div>}
    </div>
  );
}

export default function ({ currentValues }) {
  const [activeValue, setActiveValue] = useState(null)

  const onSelectValue = (value) => {
    if (value == activeValue) {
      setActiveValue(null);
    } else {
      setActiveValue(value);
    }
  }

  let elements = [];
  for (let value of Object.keys(yaml.values)) {
    elements.push(
      <ValueEntry
        valueConfig={yaml.values[value]}
        value={
          currentValues[value] == null ? {value: "-", status: "Critical"} : currentValues[value]
        }
        isActive={activeValue == value}
        onClick={() => {onSelectValue(value)}}
        key={value}
      />
    )
  }

  return (
    <div>
      {elements}
    </div>
  )
}
