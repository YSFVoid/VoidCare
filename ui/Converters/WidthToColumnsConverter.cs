using System.Globalization;
using System.Windows.Data;

namespace VoidCare.Wpf.Converters;

public sealed class WidthToColumnsConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
    {
        if (value is not double width)
        {
            return 1;
        }

        return width >= 1200 ? 2 : 1;
    }

    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
    {
        throw new NotSupportedException();
    }
}
