# Public Giscus identifiers; these are not credentials.
set(
    ONLYC_GISCUS_REPO
    "trungnt2910/OnlyC"
    CACHE
        STRING
        "Giscus discussion repository (empty disables comments)"
)
set(ONLYC_GISCUS_REPO_ID "R_kgDOVpRlJw" CACHE STRING "Giscus repository node ID")
set(ONLYC_GISCUS_CATEGORY "Songs" CACHE STRING "Giscus discussion category")
set(
    ONLYC_GISCUS_CATEGORY_ID
    "DIC_kwDOVpRlJ84DKzhN"
    CACHE
        STRING
        "Giscus discussion category node ID"
)
set(ONLYC_GISCUS_THEME "dark_dimmed" CACHE STRING "Giscus widget theme")
set(ONLYC_GISCUS_LANG "vi" CACHE STRING "Giscus widget language")

onlyc_resource_string(GISCUS_REPO VALUE "${ONLYC_GISCUS_REPO}")
onlyc_resource_string(GISCUS_REPO_ID VALUE "${ONLYC_GISCUS_REPO_ID}")
onlyc_resource_string(GISCUS_CATEGORY VALUE "${ONLYC_GISCUS_CATEGORY}")
onlyc_resource_string(GISCUS_CATEGORY_ID VALUE "${ONLYC_GISCUS_CATEGORY_ID}")
onlyc_resource_string(GISCUS_THEME VALUE "${ONLYC_GISCUS_THEME}")
onlyc_resource_string(GISCUS_LANG VALUE "${ONLYC_GISCUS_LANG}")
onlyc_resource_string(GISCUS_SITE_ORIGIN VALUE "${ONLYC_SITE_ORIGIN}")
