#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <vector>
#include <array>
#include <assert.h>

#include <windows.h>

static bool is_debug = true;

enum class ItemType
{
    None = 0,
    Outside,
    Water,
    Coast,
    Land,
    Forest,
    ThickForest,
    MysticForest
};

struct Item
{
    ItemType type;
    CHAR_INFO data;
};

struct Console
{
    short x;
    short y;
    short fontX;
    short fontY;
    HANDLE handle;
    HANDLE handleInput;
    SMALL_RECT rectWindow;
};

enum RuleType
{
    Top = 0,
    Left = 1,
    Right = 3,
    Bot = 4
};

struct Rule
{
    ItemType item1;
    ItemType item2;
    RuleType type;
};

struct World
{
    CHAR_INFO* screenBuffer;
    Console console;
    std::vector<ItemType> buffer;
    std::vector<Item> items;
    std::vector<Rule> rules;
    std::vector<int> entropy;
};

enum Color
{
    FG_BLACK = 0x0000,
    FG_DARK_BLUE = 0x0001,
    FG_DARK_GREEN = 0x0002,
    FG_DARK_CYAN = 0x0003,
    FG_DARK_RED = 0x0004,
    FG_DARK_MAGENTA = 0x0005,
    FG_DARK_YELLOW = 0x0006,
    FG_GREY = 0x0007,
    FG_DARK_GREY = 0x0008,
    FG_BLUE = 0x0009,
    FG_GREEN = 0x000A,
    FG_CYAN = 0x000B,
    FG_RED = 0x000C,
    FG_MAGENTA = 0x000D,
    FG_YELLOW = 0x000E,
    FG_WHITE = 0x000F,
    BG_BLACK = 0x0000,
    BG_DARK_BLUE = 0x0010,
    BG_DARK_GREEN = 0x0020,
    BG_DARK_CYAN = 0x0030,
    BG_DARK_RED = 0x0040,
    BG_DARK_MAGENTA = 0x0050,
    BG_DARK_YELLOW = 0x0060,
    BG_GREY = 0x0070,
    BG_DARK_GREY = 0x0080,
    BG_BLUE = 0x0090,
    BG_GREEN = 0x00A0,
    BG_CYAN = 0x00B0,
    BG_RED = 0x00C0,
    BG_MAGENTA = 0x00D0,
    BG_YELLOW = 0x00E0,
    BG_WHITE = 0x00F0,
};

bool InitConsole(Console* console, short x, short y)
{
    console->handle = GetStdHandle(STD_OUTPUT_HANDLE);
    console->handleInput = GetStdHandle(STD_INPUT_HANDLE);

    if (console->handle == INVALID_HANDLE_VALUE)
    {
        std::cout << "[ERROR]: failed to get console\n";
        return false;
    }

    console->x = x;
    console->y = y;

    // Change console visual size to a minimum so ScreenBuffer can shrink
    // below the actual visual size
    console->rectWindow = { 0, 0, 1, 1 };
    SetConsoleWindowInfo(console->handle, TRUE, &console->rectWindow);

    COORD coord = { x,y };
    if (!SetConsoleScreenBufferSize(console->handle, coord))
    {
        std::cout << "[ERROR]: failed to set console screen buffer size\n";
        return false;
    }

    if (!SetConsoleActiveScreenBuffer(console->handle))
    {
        std::cout << "[ERROR]: failed to set active screen buffer\n";
        return false;
    }

    CONSOLE_FONT_INFOEX cfi;
    cfi.cbSize = sizeof(cfi);
    cfi.nFont = 0;
    cfi.dwFontSize.X = console->fontX;
    cfi.dwFontSize.Y = console->fontY;
    cfi.FontFamily = FF_DONTCARE;
    cfi.FontWeight = FW_NORMAL;

    wcscpy_s(cfi.FaceName, L"Consolas");
    if (!SetCurrentConsoleFontEx(console->handle, FALSE, &cfi))
    {
        std::cout << "[ERROR]: failed to set console font\n";
        return false;
    }

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(console->handle, &csbi))
    {
        std::cout << "[ERROR]: failed to get console screen buffer info\n";
        return false;
    }

    if (x > csbi.dwMaximumWindowSize.X || y > csbi.dwMaximumWindowSize.Y)
    {
        std::cout << "[ERROR]: font size is too big\n";
        return false;
    }

    console->rectWindow = { 0, 0, short(console->x - 1), short(console->y - 1) };
    if (!SetConsoleWindowInfo(console->handle, TRUE, &console->rectWindow))
    {
        std::cout << "[ERROR]: failed to set console window info\n";
        return false;
    }

    // allow inputs here
    if (!SetConsoleMode(console->handleInput, ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT))
    {
        std::cout << "[ERROR]: failed to set console mode\n";
        return false;
    }

    return true;
}

bool InsideWorld(World* world, int x, int y)
{
    return x >= 0 && x < world->console.x && y >= 0 && y < world->console.y;
}

void Draw(World* world, int x, int y, char c, short color = 0x000F)
{
    if (x < 0 || x >= world->console.x || y < 0 || y >= world->console.y)
        return;

    world->screenBuffer[x + y * world->console.x].Char.UnicodeChar = c;
    world->screenBuffer[x + y * world->console.x].Attributes = color;
}

void Add(World* world, ItemType type, int x, int y)
{
    auto it = std::find_if(world->items.begin(), world->items.end(), [type](const Item& i) {return i.type == type; });
    if (it == world->items.end())
        return;

    world->buffer[x + y * world->console.x] = type;
    Draw(world, x, y, it->data.Char.UnicodeChar, it->data.Attributes);
}

void Render(World* world)
{
    WriteConsoleOutput(world->console.handle, world->screenBuffer, { world->console.x, world->console.y }, { 0, 0 }, &world->console.rectWindow);
}

void AddRule(World* world, Rule rule)
{
    world->rules.push_back(rule);

    bool doMirror = false;
    RuleType mirrorType;

    switch (rule.type)
    {
    case RuleType::Left:
    {
        doMirror = true;
        mirrorType = RuleType::Right;
    } break;
    case RuleType::Right:
    {
        doMirror = true;
        mirrorType = RuleType::Left;
    } break;
    case RuleType::Top:
    {
        doMirror = true;
        mirrorType = RuleType::Bot;
    } break;
    case RuleType::Bot:
    {
        doMirror = true;
        mirrorType = RuleType::Top;

    } break;
    }

    if (doMirror)
    {
        ItemType temp = rule.item1;
        rule.item1 = rule.item2;
        rule.item2 = temp;
        rule.type = mirrorType;

        world->rules.push_back(rule);
    }
}


void InitItems(World* world)
{
    Item i1 = {};
    i1.type = ItemType::Water;
    i1.data.Char.UnicodeChar = ' ';
    i1.data.Attributes = Color::BG_BLUE;

    Item i2 = {};
    i2.type = ItemType::Coast;
    i2.data.Char.UnicodeChar = ' ';
    i2.data.Attributes = Color::BG_YELLOW;

    Item i3 = {};
    i3.type = ItemType::Land;
    i3.data.Char.UnicodeChar = ' ';
    i3.data.Attributes = Color::BG_DARK_YELLOW;

    Item i4 = {};
    i4.type = ItemType::Forest;
    i4.data.Char.UnicodeChar = ' ';
    i4.data.Attributes = Color::BG_GREEN;

    Item i5 = {};
    i5.type = ItemType::ThickForest;
    i5.data.Char.UnicodeChar = ' ';
    i5.data.Attributes = Color::BG_DARK_GREEN;

    Item i6 = {};
    i6.type = ItemType::MysticForest;
    i6.data.Char.UnicodeChar = ' ';
    i6.data.Attributes = Color::BG_DARK_RED;

    world->items.push_back(i1);
    world->items.push_back(i2);
    world->items.push_back(i3);
    world->items.push_back(i4);
    world->items.push_back(i5);
    world->items.push_back(i6);

    // water-water
    AddRule(world, { ItemType::Water, ItemType::Water, RuleType::Left });
    AddRule(world, { ItemType::Water, ItemType::Water, RuleType::Top });

    // water-coast
    AddRule(world, { ItemType::Water, ItemType::Coast, RuleType::Left });
    AddRule(world, { ItemType::Water, ItemType::Coast, RuleType::Right });
    AddRule(world, { ItemType::Water, ItemType::Coast, RuleType::Top });
    AddRule(world, { ItemType::Water, ItemType::Coast, RuleType::Bot });

    // coast-coast

    AddRule(world, { ItemType::Coast, ItemType::Coast, RuleType::Left });
    AddRule(world, { ItemType::Coast, ItemType::Coast, RuleType::Top });

    // coast-land
    AddRule(world, { ItemType::Coast, ItemType::Land, RuleType::Left });
    AddRule(world, { ItemType::Coast, ItemType::Land, RuleType::Right });
    AddRule(world, { ItemType::Coast, ItemType::Land, RuleType::Bot });
    AddRule(world, { ItemType::Coast, ItemType::Land, RuleType::Top });

    // land-land
    AddRule(world, { ItemType::Land, ItemType::Land, RuleType::Left });
    AddRule(world, { ItemType::Land, ItemType::Land, RuleType::Bot });

    // land-forest
    AddRule(world, { ItemType::Land, ItemType::Forest, RuleType::Left });
    AddRule(world, { ItemType::Land, ItemType::Forest, RuleType::Right });
    AddRule(world, { ItemType::Land, ItemType::Forest, RuleType::Top });
    AddRule(world, { ItemType::Land, ItemType::Forest, RuleType::Bot });

    // forest-forest
    AddRule(world, { ItemType::Forest, ItemType::Forest, RuleType::Left });
    AddRule(world, { ItemType::Forest, ItemType::Forest, RuleType::Top });

    // forest-thick forest
    AddRule(world, { ItemType::Forest, ItemType::ThickForest, RuleType::Left });
    AddRule(world, { ItemType::Forest, ItemType::ThickForest, RuleType::Right });
    AddRule(world, { ItemType::Forest, ItemType::ThickForest, RuleType::Bot });
    AddRule(world, { ItemType::Forest, ItemType::ThickForest, RuleType::Top });

    // forest-mystic forest
    
    AddRule(world, { ItemType::MysticForest, ItemType::MysticForest, RuleType::Left });
    AddRule(world, { ItemType::MysticForest, ItemType::MysticForest, RuleType::Top });

    AddRule(world, { ItemType::MysticForest, ItemType::ThickForest, RuleType::Left });
    AddRule(world, { ItemType::MysticForest, ItemType::ThickForest, RuleType::Right });
    AddRule(world, { ItemType::MysticForest, ItemType::ThickForest, RuleType::Top });
    AddRule(world, { ItemType::MysticForest, ItemType::ThickForest, RuleType::Bot });

    AddRule(world, { ItemType::MysticForest, ItemType::Forest, RuleType::Bot });
    AddRule(world, { ItemType::MysticForest, ItemType::Forest, RuleType::Top });
    AddRule(world, { ItemType::MysticForest, ItemType::Forest, RuleType::Left });
    AddRule(world, { ItemType::MysticForest, ItemType::Forest, RuleType::Right });
}

std::vector<ItemType> GetAllTypesForRule(World* world, ItemType item, RuleType type)
{
    std::vector<ItemType> items;

    for (const Rule& rule : world->rules)
    {
        if (rule.item1 != item || rule.type != type)
            continue;

        items.push_back(rule.item2);
    }

    return items;
}

std::array<ItemType, 5> GetSlice(World* world, int x, int y)
{
    std::array<ItemType, 5> slice = {};
    int iter = 0;

    for (int j = -1; j < 2; j++)
    {
        for (int i = -1; i < 2; i++)
        {
            // remove diagonal
            if (abs(i) + abs(j) >= 2)
                continue;

            int posX = x + i;
            int posY = y + j;

            if (InsideWorld(world, posX, posY))
            {
                slice[iter++] = world->buffer[posX + posY * world->console.x];
            }
            else
            {
                slice[iter++] = ItemType::Outside;
            }
        }
    }

    return slice;
}

bool IsSliceEmpty(const std::array<ItemType, 5>& slice)
{
    for (ItemType type : slice)
    {
        if (type != ItemType::None && type != ItemType::Outside)
            return false;
    }

    return true;
}

std::vector<ItemType> GetListOfPossible(World* world, int x, int y)
{
    std::array<ItemType, 5> slice = GetSlice(world, x, y);
    std::vector<ItemType> possible;

    if (IsSliceEmpty(slice))
    {
        for (const Item& item : world->items)
        {
            possible.push_back(item.type);
        }

        return possible;
    }

    for (int i = 0; i < 5; i++)
    {
        if (slice[i] == ItemType::None || slice[i] == ItemType::Outside)
            continue;

        // skip the center of the Slice
        if (i == 2)
            continue;

        std::vector<ItemType> types = GetAllTypesForRule(world, slice[i], (RuleType)i);
        if (possible.empty())
        {
            possible.insert(possible.begin(), types.begin(), types.end());
        }
        else
        {
            possible.erase(std::remove_if(possible.begin(), possible.end(), [&](ItemType t)
                {
                    auto it = std::find(types.begin(), types.end(), t);
                    return it == types.end();
                }
            ), possible.end());
        }
    }

    return possible;
}

void FillPosition(World* world, int x, int y)
{
    std::vector<ItemType> possible = GetListOfPossible(world, x, y);

    if(!is_debug)
        assert(!possible.empty() && "List of possible items is empty !");
    else if(possible.empty())
    {
        Draw(world, x, y, 'X');
        return;
    }

    const int rand = std::rand() % possible.size();
    Add(world, possible[rand], x, y);
}

void UpdateEntropy(World* world, int x, int y)
{
    world->entropy[x + y * world->console.x] = INT_MAX;

    for (int j = -1; j < 2; j++)
    {
        for (int i = -1; i < 2; i++)
        {
            // remove diagonal
            if (abs(i) + abs(j) >= 2)
                continue;

            const int posX = x + i;
            const int posY = y + j;
            const int index = posX + posY * world->console.x;

            if (InsideWorld(world, posX, posY) && world->buffer[index] == ItemType::None)
            {
                world->entropy[index] = GetListOfPossible(world, posX, posY).size();
            }
        }
    }
}

int main()
{
    srand(time(0));

    World world = {};

    world.console.fontX = 0;
    world.console.fontY = 12;
    if (!InitConsole(&world.console, 60, 15))
        return 1;

    world.screenBuffer = new CHAR_INFO[world.console.x * world.console.y];
    memset(world.screenBuffer, 0, sizeof(CHAR_INFO) * world.console.x * world.console.y);
    world.buffer.resize(world.console.x * world.console.y);
    world.entropy.resize(world.console.x * world.console.y);
    std::fill(world.entropy.begin(), world.entropy.end(), INT_MAX - 1);

    InitItems(&world);

    int x = std::rand() % world.console.x;
    int y = std::rand() % world.console.y;

    while (true)
    {
        FillPosition(&world, x, y);
        UpdateEntropy(&world, x, y);

        int min = INT_MAX;
        for (int i = 0; i < world.entropy.size(); i++)
        {
            if (min > world.entropy[i])
            {
                min = world.entropy[i];
                y = i / world.console.x;
                x = i - y*world.console.x;
            }
        }

        if (min == INT_MAX)
            break;

        if (is_debug)
        {
            getchar();
            Render(&world);
        }
       
    }

    Render(&world);
    int ret = getchar();
}