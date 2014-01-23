﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;

namespace WpfApplication1
{
    /// <summary>
    /// Interaction logic for Window1.xaml
    /// </summary>
    public partial class patternWindow : Window
    {
        BitmapImage image_x = new BitmapImage(new Uri(@"D:\Dropbox\VMware_share\IR_Positioning\WPF_socket\WpfApplication1\WpfApplication1\bin\Debug\x.png"));
        BitmapImage image_y = new BitmapImage(new Uri(@"D:\Dropbox\VMware_share\IR_Positioning\WPF_socket\WpfApplication1\WpfApplication1\bin\Debug\y.png"));
        public patternWindow()
        {
            InitializeComponent();
            image1.Source = image_x;
        }
        
        private void Button_Click(object sender, RoutedEventArgs e)
        {
            image1.Source = image_x;              
        }

        private void btn2_Click(object sender, RoutedEventArgs e)
        {
            image1.Source = image_y;
        }

        public void SwitchPicture(bool index)
        { 
            if(index)
                image1.Source = image_y;
            else
                image1.Source = image_x;
        }

       
    }
}
