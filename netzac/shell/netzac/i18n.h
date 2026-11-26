#pragma once

#include <clocale>
#include <cstdlib>
#include <filesystem>
#include <libintl.h>
#include <string>

#include <QString>

#ifndef ZACOS9_TEXTDOMAIN
#define ZACOS9_TEXTDOMAIN "zacos9-netzac"
#endif

#ifndef ZACOS9_LOCALEDIR_DEFAULT
#define ZACOS9_LOCALEDIR_DEFAULT "/usr/share/locale"
#endif

inline void initI18n()
{
    std::setlocale(LC_ALL, "");
    const char *envDir = std::getenv("ZACOS9_LOCALEDIR");
    std::string chosenDir;
    if (envDir && *envDir) {
        chosenDir = envDir;
    } else {
        const char *xdgData = std::getenv("XDG_DATA_HOME");
        const char *home = std::getenv("HOME");
        std::string userLocale;
        if (xdgData && *xdgData)
            userLocale = std::string(xdgData) + "/locale";
        else if (home && *home)
            userLocale = std::string(home) + "/.local/share/locale";

        std::error_code ec;
        if (!userLocale.empty() && std::filesystem::exists(userLocale, ec))
            chosenDir = userLocale;
        else
            chosenDir = ZACOS9_LOCALEDIR_DEFAULT;
    }
    bindtextdomain(ZACOS9_TEXTDOMAIN, chosenDir.c_str());
    bind_textdomain_codeset(ZACOS9_TEXTDOMAIN, "UTF-8");
}

inline QString ztr(const char *msgid)
{
    if (!msgid || !*msgid)
        return QString();
    return QString::fromUtf8(dcgettext(ZACOS9_TEXTDOMAIN, msgid, LC_MESSAGES));
}

inline QString ztrn(const char *singular, const char *plural, unsigned long n)
{
    if (!singular || !plural)
        return QString();
    return QString::fromUtf8(dcngettext(ZACOS9_TEXTDOMAIN, singular, plural, n, LC_MESSAGES));
}
