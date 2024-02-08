import Dialog from './dialog'

const DARK_BG_COLOR = 'bg-neutral-800'
const DARK_CARD_COLOR_1 = 'bg-neutral-700'
const DARK_CARD_COLOR_2 = 'bg-neutral-600'
const DARK_ACCENT_COLOR_1 = 'bg-neutral-500'
const DARK_ACCENT_COLOR_2 = 'bg-neutral-400'
const LIGHT_BG_COLOR = 'bg-neutral-100'
const LIGHT_CARD_COLOR_1 = 'bg-white'
const LIGHT_CARD_COLOR_2 = 'bg-neutral-200'
const LIGHT_ACCENT_COLOR_1 = 'bg-neutral-300'
const LIGHT_ACCENT_COLOR_2 = 'bg-neutral-400'

function ColorMode({ mode, active, onClick }) {
    let name = ''
    let leftBgColor
    let leftCardColor1
    let leftCardColor2 = ''
    let rightBgColor
    let rightCardColor1
    let rightCardColor2 = ''
    let accentColor1
    let accentColor2 = ''
    if (mode === 'light') {
        name = 'Light'
        leftBgColor = rightBgColor = LIGHT_BG_COLOR
        leftCardColor1 = rightCardColor1 = LIGHT_CARD_COLOR_1
        leftCardColor2 = rightCardColor2 = LIGHT_CARD_COLOR_2
        accentColor1 = LIGHT_ACCENT_COLOR_1
        accentColor2 = LIGHT_ACCENT_COLOR_2
    } else if (mode === 'dark') {
        name = 'Dark'
        leftBgColor = rightBgColor = DARK_BG_COLOR
        leftCardColor1 = rightCardColor1 = DARK_CARD_COLOR_1
        leftCardColor2 = rightCardColor2 = DARK_CARD_COLOR_2
        accentColor1 = DARK_ACCENT_COLOR_1
        accentColor2 = DARK_ACCENT_COLOR_2
    } else if (mode === 'system') {
        leftBgColor = LIGHT_BG_COLOR
        leftCardColor1 = LIGHT_CARD_COLOR_1
        leftCardColor2 = LIGHT_CARD_COLOR_2
        accentColor1 = LIGHT_ACCENT_COLOR_1
        accentColor2 = LIGHT_ACCENT_COLOR_2
        rightBgColor = DARK_BG_COLOR
        rightCardColor1 = DARK_CARD_COLOR_1
        rightCardColor2 = DARK_CARD_COLOR_2
        name = 'Auto'
    }
    return (
        <div className="flex flex-col" onClick={onClick}>
            <div
                className={`flex flex-row flex-none w-36 h-24 ml-3 mr-4 mt-2 mb-1 rounded-md overflow-hidden border ${active
                    ? 'ring-blue-400 ring-2 border-transparent dark:ring-blue-300'
                    : 'border-neutral-300 dark:border-zinc-600'
                    }`}
            >
                <div className={`w-1/2 ${leftBgColor}`}>
                    <div className="flex flex-col flex-none h-8 justify-center items-end">
                        <div
                            className={`flex flex-row items-center h-4 w-3/4 rounded-l-sm ${leftCardColor1}`}
                        >
                            <div
                                className={`h-1 w-2/3 ml-2 rounded-sm ${accentColor1}`}
                            ></div>
                        </div>
                    </div>
                    <div className="flex flex-col grow items-end">
                        <div
                            className={`h-4 w-3/4 rounded-l-sm mb-[2px] flex flex-row items-center ${leftCardColor2}`}
                        >
                            <div
                                className={`flex-none w-1 h-1 ml-2 ${accentColor2}`}
                            ></div>
                            <div
                                className={`flex-none w-5 h-1 ml-2 rounded-sm ${accentColor1}`}
                            ></div>
                        </div>
                        <div
                            className={`h-4 w-3/4 rounded-l-sm mb-[2px] flex flex-row items-center ${leftCardColor2}`}
                        >
                            <div
                                className={`flex-none w-1 h-1 ml-2 ${accentColor2}`}
                            ></div>
                            <div
                                className={`flex-none w-5 h-1 ml-2 rounded-sm ${accentColor1}`}
                            ></div>
                        </div>
                        <div
                            className={`h-4 w-3/4 rounded-l-sm mb-[2px] flex flex-row items-center ${leftCardColor2}`}
                        >
                            <div
                                className={`flex-none w-1 h-1 ml-2 ${accentColor2}`}
                            ></div>
                            <div
                                className={`flex-none w-5 h-1 ml-2 rounded-sm ${accentColor1}`}
                            ></div>
                        </div>
                        <div
                            className={`h-4 w-3/4 rounded-l-sm mb-[2px] flex flex-row items-center ${leftCardColor2}`}
                        >
                            <div
                                className={`flex-none w-1 h-1 ml-2 ${accentColor2}`}
                            ></div>
                            <div
                                className={`flex-none w-5 h-1 ml-2 rounded-sm ${accentColor1}`}
                            ></div>
                        </div>
                    </div>
                </div>
                <div className={`w-1/2 ${rightBgColor}`}>
                    <div className="flex flex-col flex-none h-8 justify-center">
                        <div
                            className={`flex flex-row items-center h-4 w-3/4 rounded-r-sm ${rightCardColor1}`}
                        ></div>
                    </div>
                    <div className="flex flex-col grow">
                        <div
                            className={`h-4 w-3/4 rounded-tr-sm mb-[2px] ${rightCardColor2}`}
                        ></div>
                        <div
                            className={`h-4 w-3/4 rounded-tr-sm mb-[2px] ${rightCardColor2}`}
                        ></div>
                        <div
                            className={`h-4 w-3/4 rounded-tr-sm mb-[2px] ${rightCardColor2}`}
                        ></div>
                        <div
                            className={`h-4 w-3/4 rounded-tr-sm mb-[2px] ${rightCardColor2}`}
                        ></div>
                    </div>
                </div>
            </div>
            <p
                className={`flex-grow text-sm font-medium ml-4 ${active
                    ? 'text-blue-500 dark:text-blue-300'
                    : 'text-neutral-900 dark:text-zinc-400'
                    }`}
            >
                {name}
            </p>
        </div>
    )
}

function ColorModeSetting({ mode, setColorMode }) {
    const modes = ['light', 'dark', 'system']
    const modesJsx = modes.map((m) => {
        return (
            <ColorMode
                mode={m}
                active={mode == m}
                onClick={() => {
                    setColorMode(m)
                }}
                key={m}
            />
        )
    })
    return (
        <div className="flex flex-col">
            <p className="text-medium font-normal ml-4 dark:text-zinc-300">
                Color Theme
            </p>
            <div className="flex flex-row flex-none">{modesJsx}</div>
        </div>
    )
}

export default function ({ onExit, settings, setSetting }) {
    return (
        <Dialog onExit={onExit}>
            <div className="flex flex-col grow w-full h-full">
                <p className="flex-none text-2xl font-medium text-black mb-4 dark:text-zinc-300">
                    Settings
                </p>
                <div className="flex grow flex-row w-full overflow-auto">
                    <ColorModeSetting
                        mode={settings.theme}
                        setColorMode={(mode) => {setSetting("theme", mode); } }
                    />
                </div>
            </div>
        </Dialog>
    )
}
