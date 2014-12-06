function get_files
{
    echo kdenlive.xml
}

function po_for_file
{
    case "$1" in
       kdenlive.xml)
           echo kdenlive_xml_mimetypes.po
       ;;
    esac
}

function tags_for_file
{
    case "$1" in
       kdenlive.xml)
           echo comment
       ;;
    esac
}
