import { PiGearFineBold } from 'react-icons/pi'

function MenuItem({ active, text, onClick, showStatus }) {
  const bgColor = active ? 'bg-neutral-200 dark:bg-zinc-600' : 'bg-transparent'
  return (
    <div
      className={`flex-none flex flex-row justify-left items-center focus:bg-blue-400 cursor-default h-10 border border-transparent rounded-lg px-2 ${bgColor}`}
      onClick={onClick}
    >
      {showStatus && <div className="rounded-sm h-5 w-1 bg-green-500" />}
      <p className="text-sm font-medium ml-2 text-neutral-800 dark:text-zinc-100">{text}</p>
    </div >
  )
}

export default function ({
  menuItems,
  activeMenuItem,
  onSelectContent,
  onSettingsDialog,
  onStatusDialog,
  overallStatus
}) {
  let displayMenuItems = []
  for (let menuItem of Object.keys(menuItems)) {
    displayMenuItems.push(
      <MenuItem
        text={menuItem}
        active={menuItem==activeMenuItem}
        onClick={() => onSelectContent(menuItem)}
        showStatus={menuItems[menuItem]["showStatus"]}
        key={menuItem}
      />
    )
  }

  return (
    <div className="flex flex-col h-max items-stretch grow">
      <div className="border-b border-neutral-200 h-20 flex flex-col flex-none justify-center dark:border-zinc-700" onClick={() => {
        onStatusDialog();
      }}>
        <div
          className="flex grow flex-row items-center cursor-default border bg-neutral-100 hover:bg-white m-3 rounded-lg py-2 px-2 mb-4 mt-2 mx-4 dark:bg-zinc-800 dark:border-zinc-700 dark:hover:bg-zinc-700"
        >
          <p className="ml-4 mr-2 text-neutral-800 dark:text-zinc-300">Status {overallStatus}</p>
        </div>
      </div>
      <div className="flex flex-col justify-end p-3 grow">
        <div className="flex flex-col justify-start grow">
          {displayMenuItems}
        </div>
        <div className="border-t dark:border-zinc-700">
          <div
            className="flex flex-row items-center flex-none p-3 hover:bg-white dark:hover:bg-zinc-700 rounded-md mt-2"
            onClick={onSettingsDialog}
          >
            <div className="text-neutral-600 dark:text-zinc-300 text-md">
              <PiGearFineBold />
            </div>
            <div className="">
              <p className="text-sm font-medium text-neutral-600 ml-2 dark:text-zinc-300">
                Settings
              </p>
            </div>
          </div>
        </div>
      </div>
    </div>
  )
}
